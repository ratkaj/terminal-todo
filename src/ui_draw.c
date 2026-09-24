// lspdiag

#include <curses.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <common.h>
#include <project.h>
#include <storage.h>
#include <task.h>
#include <ui_draw.h>
#include <ui_layout.h>

/*
 * Returns the number of bytes of src (out of src_len) that fit within
 * max_cols terminal display columns, measuring by wcwidth() rather than byte
 * or character count so double-width/combining characters are accounted for
 * correctly (per docs/developer/ncurses-ui.md's "measure and truncate by
 * terminal display cells" rule). A multi-byte UTF-8 sequence is never split:
 * an incomplete or invalid sequence stops the scan before it, so a truncated
 * tail is dropped rather than emitting malformed bytes to the terminal.
 */
static size_t clip_to_cols(const char *src, size_t src_len, int max_cols, int *out_cols)
{
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));
	size_t consumed = 0;
	int cols = 0;
	while (consumed < src_len) {
		wchar_t wc;
		size_t n = mbrtowc(&wc, src + consumed, src_len - consumed, &ps);
		if (n == (size_t)-1 || n == (size_t)-2)
			break; /* invalid or incomplete sequence: stop before it */
		if (n == 0)
			break; /* embedded NUL terminates the printable content */
		int w = wcwidth(wc);
		if (w < 0)
			w = 0; /* non-printable (e.g. combining mark): zero-width */
		if (cols + w > max_cols)
			break;
		consumed += n;
		cols += w;
	}
	if (out_cols != NULL)
		*out_cols = cols;
	return consumed;
}

static int color_for_priority(priority_t p)
{
	if (p == PRIORITY_P1)
		return 1;
	if (p == PRIORITY_P2)
		return 2;
	return 0;
}

/*
 * Format into a buffer and print at most (win's actual width - x) columns.
 * mvwprintw() does not clip a string that overruns a window's right edge -
 * it wraps the overflow onto the window's next row, silently corrupting
 * whatever is drawn there. Every overlay uses this instead of mvwprintw()
 * directly so a too-long line is truncated, never wrapped.
 */
/*
 * Like put_clipped(), but also right-pads with spaces to exactly @p cols
 * display columns, for full-width reverse-video status/confirm bars where
 * the highlight must cover the whole row.
 */
static void put_clipped_padded(WINDOW *win, int y, int x, int cols, const char *s)
{
	int max_w = cols - x;
	if (max_w <= 0)
		return;
	int used_cols = 0;
	size_t nbytes = clip_to_cols(s, strlen(s), max_w, &used_cols);
	mvwprintw(win, y, x, "%.*s", (int)nbytes, s);
	/* Callers of this helper (draw_confirm/draw_reorder_status) size their
	 * window to exactly (1, COLS) at the last screen row, so this padding
	 * loop's final waddch() writes the terminal's bottom-right cell. ncurses
	 * can return ERR there even though the character was drawn correctly,
	 * because the cursor cannot advance further with scrolling disabled
	 * (docs/developer/ncurses-ui.md, "Bottom-right cell"). That return value
	 * is deliberately not checked here: it does not indicate a failed draw.
	 */
	for (int i = used_cols; i < max_w; i++)
		waddch(win, ' ');
}

static void put_clipped(WINDOW *win, int y, int x, const char *fmt, ...)
{
	int max_w = getmaxx(win) - x;
	if (max_w <= 0)
		return;

	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	size_t nbytes = clip_to_cols(buf, strlen(buf), max_w, NULL);
	mvwprintw(win, y, x, "%.*s", (int)nbytes, buf);
}

static void draw_pane_frame(WINDOW *win, const char *heading, bool focused)
{
	box(win, 0, 0);
	if (focused)
		wattron(win, A_REVERSE);
	put_clipped(win, 0, 1, " %s ", heading);
	if (focused)
		wattroff(win, A_REVERSE);
}

static WINDOW *centered_window(int h, int w)
{
	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	if (h > rows)
		h = rows;
	if (w > cols)
		w = cols;
	int y = (rows - h) / 2;
	int x = (cols - w) / 2;
	if (y < 0)
		y = 0;
	if (x < 0)
		x = 0;
	return newwin(h, w, y, x);
}

/*
 * Clip to the remaining width before printing. mvwprintw() does NOT clip a
 * string that doesn't fit before a window's right edge - it wraps the
 * overflow onto the window's next row instead, which would silently
 * corrupt the row below with leftover characters from a too-long entry.
 */
static void draw_footer_entry_at(WINDOW *win, int row, int x, int width,
	const hotkey_entry_t *e)
{
	if (x >= width)
		return;
	char buf[192];
	snprintf(buf, sizeof(buf), "%s %s", e->key, e->label);
	size_t nbytes = clip_to_cols(buf, strlen(buf), width - x, NULL);
	mvwprintw(win, row, x, "%.*s", (int)nbytes, buf);
}

static void draw_footer_entries(WINDOW *win, int width,
	const hotkey_entry_t *entries, size_t n)
{
	int widths[24];
	size_t ncols = 0, nrows = 0;
	if (n > 24)
		n = 24;
	ui_layout_footer_columns(entries, n, width, widths, 24, &ncols, &nrows);
	if (ncols == 0)
		return;

	/* Filled row by row: entry i sits in row i / ncols, column i % ncols. */
	int x = 0;
	for (size_t i = 0; i < n; i++) {
		size_t col = i % ncols;
		if (col == 0)
			x = 0;
		draw_footer_entry_at(win, (int)(i / ncols), x, width, &entries[i]);
		x += widths[col] + 3;
	}
}

static void draw_projects_pane(rect_t r, const app_state_t *st)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);
	draw_pane_frame(win, "PROJECTS", st->focus == FOCUS_PROJECTS);

	int archived = storage_project_count_archived();

	/* One row at the bottom is reserved for the archived-project count, but
	   only when there's one to show, so it doesn't cost a list row otherwise. */
	int max_rows = r.h - 2 - (archived > 0 ? 1 : 0);
	int row = 1;

	/* Selection ranges over one combined list: the provisional row (if any)
	   at index 0, then the real project list - see
	   input_dispatch.c's projects_pane_sync_preview(). */
	bool provisional_is_open = st->provisional_active && st->project_sel == 0;
	if (st->provisional_active && row - 1 < max_rows) {
		char label[PROJECT_NAME_MAX + 4];
		snprintf(label, sizeof(label), "[%s]", st->provisional_project.display_name);
		put_clipped(win, row, 1, "%s %-14.14s (new)", provisional_is_open ? ">" : " ", label);
		row++;
	}

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(st->archived_shown_projects, &arr, &n);

	/* One blank row separates built-in projects, then active user projects,
	   then archived user projects (when shown); storage_project_list()
	   already sorts builtin first and archived last within that, so this is
	   just tracking the transition points. */
	enum project_group { GROUP_BUILTIN, GROUP_ACTIVE, GROUP_ARCHIVED };
	enum project_group prev_group = GROUP_BUILTIN;
	for (size_t i = 0; i < n && row - 1 < max_rows; i++) {
		enum project_group cur_group = arr[i].builtin ? GROUP_BUILTIN
			: (arr[i].archived ? GROUP_ARCHIVED : GROUP_ACTIVE);
		if (i > 0 && cur_group != prev_group) {
			row++;
			if (row - 1 >= max_rows)
				break;
		}
		prev_group = cur_group;

		int count = storage_project_task_count(arr[i].id);
		char line[256];
		size_t combined_idx = i + (st->provisional_active ? 1 : 0);
		bool selected = (size_t)st->project_sel == combined_idx;
		snprintf(line, sizeof(line), "%s %-14.14s (%d)",
			selected ? ">" : " ", arr[i].display_name, count);
		int line_max_w = r.w > 2 ? r.w - 2 : 0;
		size_t line_nbytes = clip_to_cols(line, strlen(line), line_max_w, NULL);
		mvwprintw(win, row, 1, "%.*s", (int)line_nbytes, line);
		row++;
	}
	storage_project_array_free(arr, n);

	if (archived > 0)
		put_clipped(win, r.h - 2, 1, "Archived: %d", archived);

	wnoutrefresh(win);
	delwin(win);
}

static void draw_tasks_pane(rect_t r, const app_state_t *st)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);

	char heading[PROJECT_NAME_MAX + 16];
	project_t proj;
	if (st->current_project_id != 0
			&& storage_project_get(st->current_project_id, &proj) == RT_SUCCESS) {
		snprintf(heading, sizeof(heading), "TASKS (%s)", proj.display_name);
		project_model_free(&proj);
	} else if (st->provisional_active && st->current_project_id == 0) {
		snprintf(heading, sizeof(heading), "TASKS ([%s])", st->provisional_project.display_name);
	} else {
		snprintf(heading, sizeof(heading), "TASKS");
	}
	draw_pane_frame(win, heading, st->focus == FOCUS_TASKS);

	if (st->current_project_id != 0 || st->provisional_active) {
		task_t *arr = NULL;
		size_t n = 0;
		task_list_visible_rows(st->current_project_id, st->archived_shown_tasks, &arr, &n);

		int archived = (st->current_project_id != 0)
			? storage_task_count_archived(st->current_project_id) : 0;

		int content_w = (r.w > 2) ? r.w - 2 : 0;
		/* One row at the bottom is reserved for the archived-task count,
		   but only when there's one to show. */
		int max_rows = r.h - 2 - (archived > 0 ? 1 : 0);
		int row = 1;
		int prev_top_state = -1;

		for (size_t i = 0; i < n && row - 1 < max_rows; i++) {
			task_t *t = &arr[i];
			bool is_sub = (t->parent_id != 0);

			/* One blank row separates ACTIVE/COMPLETED/ARCHIVED top-level
			   groups (docs/archiving_and_ordering.md's "Normal Task
			   Display" example). Checked only at top-level tasks, and keyed
			   on the top-level task's own state, so a subtask never gets
			   detached from its parent by a state difference of its own -
			   docs/archiving_and_ordering.md #16 is explicit that subtask
			   grouping never breaks a subtask away from its parent. */
			if (!is_sub) {
				if (prev_top_state >= 0 && (int)t->state != prev_top_state) {
					row++;
					if (row - 1 >= max_rows)
						break;
				}
				prev_top_state = (int)t->state;
			}

			int color = color_for_priority(t->priority);
			if (color)
				wattron(win, COLOR_PAIR(color));

			const char *marker = ((int)i == st->task_sel) ? ">" : " ";
			const char *box_char = (t->status == TASK_STATUS_COMPLETED) ? "x" : " ";
			char prefix[16];
			if (is_sub)
				snprintf(prefix, sizeof(prefix), "%s     [%s] ", marker, box_char);
			else
				snprintf(prefix, sizeof(prefix), "%s [%s] ", marker, box_char);

			/* Right-align the priority label at a fixed column by padding
			   the title to fill exactly the space between the prefix and
			   the label, rather than just appending it after a
			   variable-length title (which put P1/P2/P3 at a different
			   column on every row and read as if it were part of the
			   title). Subtasks show no priority label, matching the
			   window templates. */
			char suffix[8] = "";
			if (!is_sub)
				snprintf(suffix, sizeof(suffix), " P%d", (int)t->priority);

			int prefix_len = (int)strlen(prefix);
			int suffix_len = (int)strlen(suffix);
			int right_margin = is_sub ? 0 : 2; /* breathing room before the pane border */
			int title_w = content_w - prefix_len - suffix_len - right_margin;
			if (title_w < 1)
				title_w = 1;

			char titlebuf[TASK_TITLE_MAX];
			snprintf(titlebuf, sizeof(titlebuf), "%-*.*s", title_w, title_w, t->title);

			char line[TASK_TITLE_MAX + 32];
			snprintf(line, sizeof(line), "%s%s%s", prefix, titlebuf, suffix);
			size_t line_nbytes = clip_to_cols(line, strlen(line), content_w, NULL);
			mvwprintw(win, row, 1, "%.*s", (int)line_nbytes, line);

			if (color)
				wattroff(win, COLOR_PAIR(color));

			row++;
		}
		storage_task_array_free(arr, n);

		if (archived > 0)
			put_clipped(win, r.h - 2, 1, "Archived: %d", archived);
	}

	wnoutrefresh(win);
	delwin(win);
}

/*
 * Word-wrap @p text into @p win starting at row @p start_row, one line per
 * screen row up to (but excluding) @p max_row, breaking each source line
 * ("\n"-separated, blank lines preserved) at the last space at-or-before
 * @p content_w columns, or hard-breaking a single word longer than
 * content_w. No scrolling: text beyond max_row is simply not shown yet.
 *
 * @return The row after the last line drawn (== @p start_row if @p text is
 * NULL/empty), so a caller can stack more wrapped text right below it.
 */
static int draw_wrapped_text(WINDOW *win, int start_row, int max_row, int x,
	int content_w, const char *text)
{
	if (text == NULL || content_w <= 0)
		return start_row;

	int row = start_row;
	const char *p = text;
	while (*p != '\0' && row < max_row) {
		const char *nl = strchr(p, '\n');
		size_t para_len = nl ? (size_t)(nl - p) : strlen(p);

		if (para_len == 0) {
			row++; /* blank line: preserve the paragraph break */
		} else {
			size_t off = 0;
			while (off < para_len && row < max_row) {
				size_t remaining = para_len - off;
				size_t max_bytes = clip_to_cols(p + off, remaining, content_w, NULL);
				size_t take;
				if (max_bytes >= remaining) {
					take = remaining; /* whole rest of the paragraph fits */
				} else {
					/* Prefer breaking at the last space within the columns
					 * that fit; max_bytes is already a safe, sequence-
					 * preserving byte count from clip_to_cols(), so falling
					 * back to it (no space found) never splits a multi-byte
					 * character. */
					size_t k = max_bytes;
					while (k > 0 && p[off + k - 1] != ' ')
						k--;
					take = (k > 0) ? k : max_bytes;
				}

				char buf[512];
				size_t copy_len = (take < sizeof(buf) - 1) ? take : sizeof(buf) - 1;
				memcpy(buf, p + off, copy_len);
				buf[copy_len] = '\0';
				mvwprintw(win, row, x, "%s", buf);
				row++;

				off += take;
				if (off < para_len && p[off] == ' ')
					off++; /* skip the space we wrapped on */
			}
		}

		if (nl == NULL)
			break;
		p = nl + 1;
	}
	return row;
}

static void draw_notes_pane(rect_t r, const app_state_t *st)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);
	draw_pane_frame(win, "NOTES", st->focus == FOCUS_NOTES);

	int content_w = (r.w > 2) ? r.w - 2 : 0;

	if (st->current_project_id != 0) {
		task_t t;
		if (task_get_visible_row(st->current_project_id, st->archived_shown_tasks,
				st->task_sel, &t) == RT_SUCCESS) {
			/* The task title can be as long as any note line, so it's
			   word-wrapped the same way instead of clipped to one line -
			   an overrun used to just disappear off the pane's edge. */
			int title_end = draw_wrapped_text(win, 1, r.h - 1, 1, content_w, t.title);
			if (t.notes != NULL)
				draw_wrapped_text(win, title_end + 1, r.h - 1, 1, content_w, t.notes);
			task_model_free(&t);
		}
	}

	wnoutrefresh(win);
	delwin(win);
}

/* Footer entries for the focused pane; the order is the reading order. */
static size_t footer_entries(const app_state_t *st, hotkey_entry_t *entries)
{
	size_t ne = 0;
	entries[ne++] = (hotkey_entry_t){ "<-/->", "Panes" };
	entries[ne++] = (hotkey_entry_t){ "up/dn", "Navigate" };
	entries[ne++] = (hotkey_entry_t){ "p", "Projects" };

	if (st->focus == FOCUS_PROJECTS)
		entries[ne++] = (hotkey_entry_t){ "i", "New project" };
	else if (st->focus == FOCUS_TASKS)
		entries[ne++] = (hotkey_entry_t){ "i", "Insert" };
	else
		entries[ne++] = (hotkey_entry_t){ "i", "Edit notes" };

	if (st->focus == FOCUS_TASKS) {
		entries[ne++] = (hotkey_entry_t){ "n", "Edit notes" };
		entries[ne++] = (hotkey_entry_t){ "Space", "Done" };
		entries[ne++] = (hotkey_entry_t){ "a", st->archived_shown_tasks ? "Restore" : "Archive completed" };
		entries[ne++] = (hotkey_entry_t){ "A", st->archived_shown_tasks ? "Hide archived" : "Display archived" };
		entries[ne++] = (hotkey_entry_t){ "1/2/3", "Priority" };
		entries[ne++] = (hotkey_entry_t){ "o", "Order" };
		entries[ne++] = (hotkey_entry_t){ "m", "Move" };
		entries[ne++] = (hotkey_entry_t){ "s", "Subtask" };
		entries[ne++] = (hotkey_entry_t){ "e", "Export" };
	} else if (st->focus == FOCUS_PROJECTS) {
		entries[ne++] = (hotkey_entry_t){ "r", "Rename" };
		entries[ne++] = (hotkey_entry_t){ "a", st->archived_shown_projects ? "Restore" : "Archive project" };
		entries[ne++] = (hotkey_entry_t){ "A", st->archived_shown_projects ? "Hide archived" : "Display archived" };
	} else {
		entries[ne++] = (hotkey_entry_t){ "c", "Copy" };
	}
	entries[ne++] = (hotkey_entry_t){ "Enter", "Open/Edit" };
	entries[ne++] = (hotkey_entry_t){ "d", "Delete" };
	entries[ne++] = (hotkey_entry_t){ "?", "Help" };
	entries[ne++] = (hotkey_entry_t){ "q", "Quit" };
	return ne;
}

static void draw_footer(rect_t r, const hotkey_entry_t *entries, size_t ne)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);
	draw_footer_entries(win, r.w, entries, ne);
	wnoutrefresh(win);
	delwin(win);
}

static void draw_task_form(const app_state_t *st)
{
	const task_form_state_t *f = &st->task_form;
	int h = 9;
	int w = 64;
	WINDOW *win = centered_window(h, w);
	box(win, 0, 0);

	const char *title = f->is_new ? (f->is_subtask ? "New subtask" : "New task")
	                               : (f->is_subtask ? "Edit subtask" : "Edit task");
	put_clipped(win, 0, 2, " %s ", title);

	int row = 1;
	if (f->is_subtask)
		put_clipped(win, row++, 2, "Parent: %.50s", f->parent_title);

	/* Reverse-video the whole line of whichever field currently has focus -
	   without this, Tab/Down moving focus to Priority is invisible until the
	   user also presses Left/Right/a digit and sees the bracket move,
	   easily leaving them unsure whether the field switch actually
	   happened. */
	char namebuf[TASK_TITLE_MAX + 4];
	snprintf(namebuf, sizeof(namebuf), "%.*s|%s", (int)f->cursor, f->name, f->name + f->cursor);
	if (f->field == TASK_FORM_FIELD_NAME)
		wattron(win, A_REVERSE);
	put_clipped(win, row++, 2, "Name:     [%.45s]", namebuf);
	if (f->field == TASK_FORM_FIELD_NAME)
		wattroff(win, A_REVERSE);

	if (f->field == TASK_FORM_FIELD_PRIORITY)
		wattron(win, A_REVERSE);
	put_clipped(win, row++, 2, "Priority: %s %s %s",
		f->priority == PRIORITY_P1 ? "[P1]" : "P1",
		f->priority == PRIORITY_P2 ? "[P2]" : "P2",
		f->priority == PRIORITY_P3 ? "[P3]" : "P3");
	if (f->field == TASK_FORM_FIELD_PRIORITY)
		wattroff(win, A_REVERSE);

	if (f->error[0] != '\0')
		put_clipped(win, row++, 2, "%s", f->error);

	put_clipped(win, h - 3, 2, "Up/Dn Field    Tab Cycle    Left/Right Cursor/Priority");
	put_clipped(win, h - 2, 2, "Enter Save     Esc Cancel");

	wnoutrefresh(win);
	delwin(win);
}

static void draw_project_form(const app_state_t *st)
{
	const project_form_state_t *f = &st->project_form;
	int h = 6;
	int w = 60;
	WINDOW *win = centered_window(h, w);
	box(win, 0, 0);
	put_clipped(win, 0, 2, " %s ", f->is_rename ? "Rename Project" : "New Project");

	char namebuf[PROJECT_NAME_MAX + 4];
	snprintf(namebuf, sizeof(namebuf), "%.*s|%s", (int)f->cursor, f->name, f->name + f->cursor);
	put_clipped(win, 1, 2, "Project name: [%.35s]", namebuf);

	if (f->error[0] != '\0')
		put_clipped(win, 2, 2, "%s", f->error);

	put_clipped(win, h - 2, 2, "Left/Right Cursor   Enter Save   Esc Cancel");

	wnoutrefresh(win);
	delwin(win);
}

static void draw_confirm(const app_state_t *st)
{
	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	WINDOW *win = newwin(1, cols, rows - 1, 0);
	wattron(win, A_REVERSE);
	put_clipped_padded(win, 0, 0, cols, st->pending_confirm.message);
	wattroff(win, A_REVERSE);
	wnoutrefresh(win);
	delwin(win);
}

/*
 * Reorder mode makes no other visible change (the moved task still just
 * shows '>', same as plain selection), so without this a keypress that does
 * nothing (e.g. 'q', which is intentionally inert here - only Up/Down/Enter/
 * Esc are handled) looks exactly like the whole program has frozen. This
 * status line is the only indicator that Up/Down/Enter/Esc are being waited
 * for, and replaces the normal footer/hotkey row while active.
 */
static void draw_reorder_status(const app_state_t *st)
{
	(void)st;
	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	WINDOW *win = newwin(1, cols, rows - 1, 0);
	wattron(win, A_REVERSE);
	put_clipped_padded(win, 0, 0, cols, "ORDER -- Up/Down Move    Enter/Esc Finish");
	wattroff(win, A_REVERSE);
	wnoutrefresh(win);
	delwin(win);
}

static void draw_help(const app_state_t *st)
{
	(void)st;
	hotkey_entry_t entries[] = {
		{ "<-/->", "Panes" },        { "up/dn", "Navigate" },   { "p", "Projects" },
		{ "i", "Insert/Edit" },      { "n", "Edit notes" },     { "s", "Subtask" },
		{ "Enter", "Open/Edit" },    { "Space", "Done" },       { "d", "Delete/Clear" },
		{ "1/2/3", "Priority" },     { "o", "Order" },          { "r", "Rename" },
		{ "a", "Archive/Restore" },  { "A", "Display/Hide archived" },
		{ "c", "Copy notes" },       { "e", "Export" },         { "m", "Move to project" },
		{ "Esc", "Save/Cancel" },    { "q", "Quit" },           { "?", "Close" },
	};
	size_t n = sizeof(entries) / sizeof(entries[0]);

	int w = 94; /* 1 margin + 3 columns * 30 + 1 margin; clamped to terminal width below */
	int h = 2 + (int)((n + 2) / 3) + 2;
	WINDOW *win = centered_window(h, w);
	int actual_w = getmaxx(win);
	box(win, 0, 0);
	mvwprintw(win, 0, 2, " Help ");

	/* Three entries per line; draw_footer_entry_at() clips each to the
	   actual (possibly narrower-than-requested) window width so a long
	   entry can never wrap onto - and corrupt - the row below. */
	int row = 1;
	for (size_t i = 0; i < n; i += 3) {
		int x = 1;
		for (size_t c = i; c < i + 3 && c < n; c++) {
			draw_footer_entry_at(win, row, x, actual_w - 1, &entries[c]);
			x += 30;
		}
		row++;
	}

	wnoutrefresh(win);
	delwin(win);
}

static void draw_switcher(const app_state_t *st)
{
	int h = 12;
	int w = 50;
	WINDOW *win = centered_window(h, w);
	box(win, 0, 0);
	put_clipped(win, 0, 2, " Switch project ");
	put_clipped(win, 1, 2, "> %.40s", st->switcher_query);

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_search(st->switcher_query, false, &arr, &n);

	int max_rows = h - 4;
	for (size_t i = 0; i < n && (int)i < max_rows; i++) {
		int count = storage_project_task_count(arr[i].id);
		put_clipped(win, 3 + (int)i, 2, "%s %-20.20s %3d open",
			((int)i == st->switcher_sel) ? ">" : " ", arr[i].display_name, count);
	}
	storage_project_array_free(arr, n);

	wnoutrefresh(win);
	delwin(win);
}

static void draw_task_move(const app_state_t *st)
{
	int h = 14;
	int w = 50;
	WINDOW *win = centered_window(h, w);
	h = getmaxy(win);

	put_clipped(win, 1, 2, "Task: %s", st->task_move.title);

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list_move_targets(st->current_project_id, &arr, &n);

	/* Rows 3..h-4 hold the list; scroll so the highlighted row stays visible. */
	int max_rows = h - 6;
	int first = 0;
	if (max_rows > 0 && st->task_move.sel >= max_rows)
		first = st->task_move.sel - max_rows + 1;
	if (n == 0)
		put_clipped(win, 3, 2, "No other projects");
	for (int r = 0; r < max_rows && (size_t)(first + r) < n; r++) {
		int i = first + r;
		put_clipped(win, 3 + r, 2, "%s %s",
			(i == st->task_move.sel) ? ">" : " ", arr[i].display_name);
	}
	storage_project_array_free(arr, n);

	put_clipped(win, h - 2, 2, "Up/Down Select  Enter Move  Esc Cancel");

	/* Boxed last so a clipped line can never overwrite the right border. */
	box(win, 0, 0);
	put_clipped(win, 0, 2, " Move task ");

	wnoutrefresh(win);
	delwin(win);
}

int ui_draw_init(void)
{
	setlocale(LC_ALL, "");
	if (initscr() == NULL) {
		LERR("ui_draw_init: initscr failed");
		return RT_ERROR;
	}
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	/* ncurses' default ~1s ESCDELAY (to disambiguate a lone Esc from the
	   start of a function/arrow-key escape sequence) makes Esc feel
	   sluggish for a plain cancel key; 25ms is imperceptible for a human
	   keypress but still enough to catch a real escape sequence. */
	set_escdelay(25);

	if (has_colors()) {
		start_color();
		use_default_colors();
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_YELLOW, -1);
	} else {
		LWARN("ui_draw_init: terminal has no color support; priority labels remain text-only");
	}
	return RT_SUCCESS;
}

void ui_draw_shutdown(void)
{
	endwin();
}

void ui_draw_frame(const app_state_t *st)
{
	if (st == NULL)
		return;

	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	layout_tier_t tier = ui_layout_tier(rows, cols);
	hotkey_entry_t footer[20];
	size_t footer_n = footer_entries(st, footer);
	layout_geom_t geom;
	ui_layout_compute(rows, cols, tier, st->focus,
		ui_layout_footer_height(footer, footer_n, cols), &geom);

	/* erase()+wnoutrefresh(stdscr) must be staged before the pane
	   sub-windows: wnoutrefresh() layers onto the shared virtual screen in
	   call order, so refreshing stdscr afterward would blank out everything
	   the panes just drew. */
	erase();
	wnoutrefresh(stdscr);

	if (geom.projects.w > 0 && geom.projects.h > 0)
		draw_projects_pane(geom.projects, st);
	if (geom.tasks.w > 0 && geom.tasks.h > 0)
		draw_tasks_pane(geom.tasks, st);
	if (geom.notes.w > 0 && geom.notes.h > 0)
		draw_notes_pane(geom.notes, st);
	if (geom.footer.h > 0 && geom.footer.w > 0)
		draw_footer(geom.footer, footer, footer_n);

	switch (st->mode) {
	case MODE_TASK_FORM:        draw_task_form(st); break;
	case MODE_PROJECT_FORM:     draw_project_form(st); break;
	case MODE_CONFIRM:          draw_confirm(st); break;
	case MODE_HELP:             draw_help(st); break;
	case MODE_PROJECT_SWITCHER: draw_switcher(st); break;
	case MODE_TASK_MOVE:        draw_task_move(st); break;
	case MODE_REORDER:          draw_reorder_status(st); break;
	default: break;
	}

	doupdate();
}
