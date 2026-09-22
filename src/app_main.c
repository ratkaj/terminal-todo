// lspdiag

#include <curses.h>
#include <stdbool.h>
#include <stdlib.h>

#include <app_main.h>
#include <common.h>
#include <input_dispatch.h>
#include <notes_editor.h>
#include <project.h>
#include <storage.h>
#include <task.h>
#include <ui_draw.h>
#include <ui_state.h>

static void handle_edit_notes(app_state_t *st)
{
	if (st->current_project_id == 0)
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
		}
		/* Provisional (unregistered directory) projects are not persisted
		   until their first task is created; that atomic commit is wired up
		   alongside the task-creation form in a later milestone. */
		project_model_free(&resolved);
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

		ui_draw_frame(&st);
	}

	ui_draw_shutdown();
	storage_close();
	return RT_SUCCESS;
}
