// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <export.h>
#include <storage.h>
#include <strbuf.h>
#include <task.h>

/* Title column: "[ ] P1  " is 8 cells; subtasks are indented 4 more. Notes
   and the completion line hang under the title at the same column. */
#define EXPORT_INDENT_TOP 8
#define EXPORT_INDENT_SUB 12

/* Notes are user-authored: print each line verbatim behind a "| " gutter,
   without re-wrapping, and drop trailing blank lines. */
static void append_notes(strbuf_t *sb, const char *notes, int indent)
{
	if (notes == NULL)
		return;

	size_t end = strlen(notes);
	while (end > 0 && (notes[end - 1] == '\n' || notes[end - 1] == '\r'
			|| notes[end - 1] == ' ' || notes[end - 1] == '\t'))
		end--;

	size_t start = 0;
	while (start < end) {
		const char *nl = memchr(notes + start, '\n', end - start);
		size_t line_end = (nl != NULL) ? (size_t)(nl - notes) : end;
		size_t len = line_end - start;
		if (len > 0 && notes[start + len - 1] == '\r')
			len--;
		if (len == 0)
			sb_appendf(sb, "%*s|\n", indent, "");
		else
			sb_appendf(sb, "%*s| %.*s\n", indent, "", (int)len, notes + start);
		start = line_end + 1;
	}
}

static bool has_notes(const task_t *t)
{
	if (t->notes == NULL)
		return false;
	for (const char *c = t->notes; *c; c++)
		if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r')
			return true;
	return false;
}

static void append_task_line(strbuf_t *sb, const task_t *t, bool subtask)
{
	sb_appendf(sb, "%s[%c] P%d  %s%s\n", subtask ? "    " : "",
		t->status == TASK_STATUS_COMPLETED ? 'x' : ' ', (int)t->priority, t->title,
		t->archived ? "  (archived)" : "");
}

int export_project_text(int64_t project_id, bool include_archived, time_t now,
	char **out_text)
{
	RETURN_ERR_IF(out_text == NULL, "export_project_text: out_text is NULL");

	project_t p;
	RETURN_ERR_IF(storage_project_get(project_id, &p) != RT_SUCCESS,
		"export_project_text: project %lld not found", (long long)project_id);

	task_t *rows = NULL;
	size_t n = 0;
	if (task_list_visible_rows(project_id, include_archived, &rows, &n) != RT_SUCCESS) {
		LERR("export_project_text: listing tasks failed");
		project_model_free(&p);
		return RT_ERROR;
	}

	/* Top-level counts only, matching the Projects pane. */
	int n_open = 0, n_done = 0, n_archived = 0;
	for (size_t i = 0; i < n; i++) {
		if (rows[i].parent_id != 0)
			continue;
		if (rows[i].state == TASK_STATE_ARCHIVED)
			n_archived++;
		else if (rows[i].state == TASK_STATE_COMPLETED)
			n_done++;
		else
			n_open++;
	}

	strbuf_t sb = {0};
	char stamp[32];
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm);

	sb_appendf(&sb, "%s\n", p.display_name);
	if (p.canonical_path != NULL)
		sb_appendf(&sb, "Path:     %s\n", p.canonical_path);
	sb_appendf(&sb, "Exported: %s\n", stamp);
	sb_appendf(&sb, "Tasks:    %d open, %d completed", n_open, n_done);
	if (include_archived)
		sb_appendf(&sb, ", %d archived", n_archived);
	sb_appendf(&sb, "\n\n");

	if (n == 0)
		sb_appendf(&sb, "(no tasks)\n");

	/* A block is a top-level task plus its notes, completion date, and
	   subtasks. Separate blocks with a blank line when either neighbour has
	   more than its title line, so runs of bare tasks stay compact. */
	bool prev_rich = false;
	for (size_t i = 0; i < n;) {
		const task_t *top = &rows[i];
		size_t j = i + 1;
		while (j < n && rows[j].parent_id != 0)
			j++;

		bool dated = (top->status == TASK_STATUS_COMPLETED && top->completed_at != 0);
		bool rich = (j > i + 1) || has_notes(top) || dated;
		if (i > 0 && (rich || prev_rich))
			sb_appendf(&sb, "\n");

		append_task_line(&sb, top, false);
		append_notes(&sb, top->notes, EXPORT_INDENT_TOP);
		if (dated) {
			char day[16];
			struct tm ctm;
			localtime_r(&top->completed_at, &ctm);
			strftime(day, sizeof(day), "%Y-%m-%d", &ctm);
			sb_appendf(&sb, "%*s(completed %s)\n", EXPORT_INDENT_TOP, "", day);
		}
		for (size_t k = i + 1; k < j; k++) {
			append_task_line(&sb, &rows[k], true);
			append_notes(&sb, rows[k].notes, EXPORT_INDENT_SUB);
		}

		prev_rich = rich;
		i = j;
	}

	storage_task_array_free(rows, n);
	project_model_free(&p);

	if (sb.failed) {
		LERR("export_project_text: out of memory");
		free(sb.buf);
		return RT_ERROR;
	}
	*out_text = sb.buf;
	return RT_SUCCESS;
}
