// lspdiag

#include <curses.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <project.h>
#include <storage.h>
#include <task.h>
#include <ui_draw.h>
#include <ui_layout.h>

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

	mvwprintw(win, y, x, "%.*s", max_w, buf);
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
	mvwprintw(win, row, x, "%.*s", width - x, buf);
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

	size_t row1_count = (nrows <= 1) ? n : (n + 1) / 2;

	int x = 0;
	for (size_t c = 0; c < row1_count; c++) {
		draw_footer_entry_at(win, 0, x, width, &entries[c]);
		x += widths[c] + 3;
	}
	if (nrows > 1) {
		x = 0;
		for (size_t c = row1_count; c < n; c++) {
			size_t col = c - row1_count;
			draw_footer_entry_at(win, 1, x, width, &entries[c]);
			x += widths[col] + 3;
		}
	}
}

static void draw_projects_pane(rect_t r, const app_state_t *st)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);
	draw_pane_frame(win, "PROJECTS", st->focus == FOCUS_PROJECTS);

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(st->archived_shown_projects, &arr, &n);

	int max_rows = r.h - 2;
	for (size_t i = 0; i < n && (int)i < max_rows; i++) {
		int count = storage_project_task_count(arr[i].id);
		char line[256];
		snprintf(line, sizeof(line), "%s %-14.14s (%d)",
			((int)i == st->project_sel) ? ">" : " ", arr[i].display_name, count);
		mvwprintw(win, (int)i + 1, 1, "%.*s", r.w > 2 ? r.w - 2 : 0, line);
	}
	storage_project_array_free(arr, n);

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
	} else {
		snprintf(heading, sizeof(heading), "TASKS");
	}
	draw_pane_frame(win, heading, st->focus == FOCUS_TASKS);

	if (st->current_project_id != 0) {
		task_t *arr = NULL;
		size_t n = 0;
		task_list_visible_rows(st->current_project_id, st->archived_shown_tasks, &arr, &n);

		int content_w = (r.w > 2) ? r.w - 2 : 0;
		int max_rows = r.h - 2;
		for (size_t i = 0; i < n && (int)i < max_rows; i++) {
			task_t *t = &arr[i];
			bool is_sub = (t->parent_id != 0);
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
			mvwprintw(win, (int)i + 1, 1, "%.*s", content_w, line);

			if (color)
				wattroff(win, COLOR_PAIR(color));
		}
		storage_task_array_free(arr, n);
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
 */
static void draw_wrapped_text(WINDOW *win, int start_row, int max_row, int x,
	int content_w, const char *text)
{
	if (text == NULL || content_w <= 0)
		return;

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
				size_t take = remaining;
				if (take > (size_t)content_w) {
					size_t k = (size_t)content_w;
					while (k > 0 && p[off + k - 1] != ' ')
						k--;
					take = (k > 0) ? k : (size_t)content_w;
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
			put_clipped(win, 1, 1, "%s", t.title);
			if (t.notes != NULL)
				draw_wrapped_text(win, 3, r.h - 1, 1, content_w, t.notes);
			task_model_free(&t);
		}
	}

	wnoutrefresh(win);
	delwin(win);
}

static void draw_footer(rect_t r, const app_state_t *st)
{
	WINDOW *win = newwin(r.h, r.w, r.y, r.x);

	hotkey_entry_t entries[20];
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
		entries[ne++] = (hotkey_entry_t){ "Space", "Done" };
		entries[ne++] = (hotkey_entry_t){ "a", st->archived_shown_tasks ? "Restore" : "Archive completed" };
		entries[ne++] = (hotkey_entry_t){ "A", st->archived_shown_tasks ? "Hide archived" : "Display archived" };
		entries[ne++] = (hotkey_entry_t){ "1/2/3", "Priority" };
		entries[ne++] = (hotkey_entry_t){ "o", "Order" };
		entries[ne++] = (hotkey_entry_t){ "s", "Subtask" };
	} else if (st->focus == FOCUS_PROJECTS) {
		entries[ne++] = (hotkey_entry_t){ "n", "New project" };
		entries[ne++] = (hotkey_entry_t){ "r", "Rename" };
		entries[ne++] = (hotkey_entry_t){ "a", st->archived_shown_projects ? "Restore" : "Archive project" };
		entries[ne++] = (hotkey_entry_t){ "A", st->archived_shown_projects ? "Hide archived" : "Display archived" };
	}
	entries[ne++] = (hotkey_entry_t){ "Enter", "Open/Edit" };
	entries[ne++] = (hotkey_entry_t){ "d", "Delete" };
	entries[ne++] = (hotkey_entry_t){ "?", "Help" };
	entries[ne++] = (hotkey_entry_t){ "q", "Quit" };

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
	mvwprintw(win, 0, 0, "%-.*s", cols, st->pending_confirm.message);
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
	mvwprintw(win, 0, 0, "%-.*s", cols, "ORDER -- Up/Down Move    Enter/Esc Finish");
	wattroff(win, A_REVERSE);
	wnoutrefresh(win);
	delwin(win);
}

static void draw_help(const app_state_t *st)
{
	(void)st;
	hotkey_entry_t entries[] = {
		{ "<-/->", "Panes" },        { "up/dn", "Navigate" },   { "p", "Projects" },
		{ "i", "Insert/Edit" },      { "n", "New project" },    { "s", "Subtask" },
		{ "Enter", "Open/Edit" },    { "Space", "Done" },       { "d", "Delete/Clear" },
		{ "1/2/3", "Priority" },     { "o", "Order" },          { "r", "Rename" },
		{ "a", "Archive/Restore" },  { "A", "Display/Hide archived" },
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
	layout_geom_t geom;
	ui_layout_compute(rows, cols, tier, st->focus, &geom);

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
		draw_footer(geom.footer, st);

	switch (st->mode) {
	case MODE_TASK_FORM:        draw_task_form(st); break;
	case MODE_PROJECT_FORM:     draw_project_form(st); break;
	case MODE_CONFIRM:          draw_confirm(st); break;
	case MODE_HELP:             draw_help(st); break;
	case MODE_PROJECT_SWITCHER: draw_switcher(st); break;
	case MODE_REORDER:          draw_reorder_status(st); break;
	default: break;
	}

	doupdate();
}
