// lspdiag

#include <ctype.h>
#include <curses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <app_main.h>
#include <common.h>
#include <export.h>
#include <input_dispatch.h>
#include <notes_editor.h>
#include <project.h>
#include <report.h>
#include <storage.h>
#include <task.h>
#include <ui_draw.h>
#include <ui_state.h>

static void handle_edit_notes(app_state_t *st)
{
	if (st->current_project_id == 0 && !st->provisional_active)
		return;

	task_t t;
	if (task_get_visible_row(st->current_project_id, st->archived_shown_tasks,
			st->task_sel, &t) != RT_SUCCESS) {
		LERR("handle_edit_notes: no task selected at index %d", st->task_sel);
		return;
	}

	char *new_text = NULL;
	if (notes_editor_edit(t.notes, &new_text) == RT_SUCCESS) {
		if (task_update_fields(t.id, NULL, new_text) != RT_SUCCESS) {
			/* Save failed: try once more with the same text so nothing is lost. */
			char *retry_text = NULL;
			if (notes_editor_edit(new_text, &retry_text) == RT_SUCCESS) {
				free(new_text);
				new_text = retry_text;
				task_update_fields(t.id, NULL, new_text);
			}
		}
		free(new_text);
	}
	task_model_free(&t);
}

static void handle_copy_notes(app_state_t *st)
{
	if (st->current_project_id == 0)
		return;

	task_t t;
	if (task_get_visible_row(st->current_project_id, st->archived_shown_tasks,
			st->task_sel, &t) != RT_SUCCESS) {
		LERR("handle_copy_notes: no task selected at index %d", st->task_sel);
		return;
	}

	notes_editor_copy_clipboard(t.notes != NULL ? t.notes : "");
	task_model_free(&t);
}

static void handle_export(app_state_t *st)
{
	if (st->current_project_id == 0)
		return;

	project_t p;
	if (storage_project_get(st->current_project_id, &p) != RT_SUCCESS) {
		LERR("handle_export: project %lld not found", (long long)st->current_project_id);
		return;
	}

	char *text = NULL;
	if (export_project_text(st->current_project_id, st->archived_shown_tasks,
			time(NULL), &text) == RT_SUCCESS) {
		char hint[PROJECT_NAME_MAX + 8];
		snprintf(hint, sizeof(hint), "export_%s", p.display_name);
		notes_editor_view(text, hint);
		free(text);
	}
	project_model_free(&p);
}

static void handle_report(const app_state_t *st)
{
	report_period_t period = (report_period_t)st->report_sel;
	char *text = NULL;
	if (report_completed_text(period, time(NULL), &text) != RT_SUCCESS)
		return;

	/* "This week" -> "report_this_week" for the temp-file name. */
	char hint[32] = "report_";
	size_t len = strlen(hint);
	for (const char *c = report_period_label(period); *c && len + 1 < sizeof(hint); c++)
		hint[len++] = (*c == ' ') ? '_' : (char)tolower((unsigned char)*c);
	hint[len] = '\0';

	notes_editor_view(text, hint);
	free(text);
}

int app_main_run(void)
{
	RETURN_ERR_IF(storage_open(NULL) != RT_SUCCESS, "app_main_run: storage_open failed");

	app_state_t st;
	app_state_init(&st);

	project_t resolved = {0};
	bool is_provisional = false;
	if (project_resolve_or_provisional(&resolved, &is_provisional) == RT_SUCCESS) {
		if (!is_provisional) {
			st.current_project_id = resolved.id;
			/* Keep the Projects pane's selection in sync with the project
			   that's actually open, rather than leaving it at index 0. */
			int idx = project_find_index(st.archived_shown_projects, resolved.id);
			if (idx >= 0)
				st.project_sel = idx;
			project_model_free(&resolved);
		} else {
			/* Not persisted until its first task is created - see
			   input_dispatch.c's task-form Enter handling, which calls
			   project_commit_provisional_with_task(). Ownership of
			   resolved.canonical_path transfers into st.provisional_project,
			   so it must not also be freed here. */
			st.provisional_active = true;
			st.provisional_project = resolved;
		}
	}

	if (ui_draw_init() != RT_SUCCESS) {
		storage_close();
		return RT_ERROR;
	}

	ui_draw_frame(&st);

	int key;
	while ((key = wgetch(stdscr)) != ERR) {
		if (key == KEY_RESIZE) {
			ui_draw_frame(&st);
			continue;
		}

		int rows, cols;
		getmaxyx(stdscr, rows, cols);
		dispatch_result_t action = input_dispatch_key(key, &st, ui_layout_tier(rows, cols));

		if (action == ACTION_QUIT)
			break;
		if (action == ACTION_EDIT_NOTES)
			handle_edit_notes(&st);
		if (action == ACTION_COPY_NOTES)
			handle_copy_notes(&st);
		if (action == ACTION_EXPORT)
			handle_export(&st);
		if (action == ACTION_REPORT)
			handle_report(&st);

		ui_draw_frame(&st);
	}

	if (st.provisional_active)
		project_model_free(&st.provisional_project);

	ui_draw_shutdown();
	storage_close();
	return RT_SUCCESS;
}
