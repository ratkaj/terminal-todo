// lspdiag

#include <curses.h> /* KEY_* integer constants only; no ncurses function is called here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <input_dispatch.h>
#include <project.h>
#include <project_resolve.h>
#include <storage.h>
#include <task.h>

enum {
	CONFIRM_ACTION_DELETE_PROJECT_CASCADE = 1,
	CONFIRM_ACTION_CLEAR_PROJECT_TASKS,
	CONFIRM_ACTION_DELETE_TASK,
	CONFIRM_ACTION_CLEAR_NOTES,
	CONFIRM_ACTION_ARCHIVE_COMPLETED,
	CONFIRM_ACTION_COMPLETE_CASCADE,
	CONFIRM_ACTION_UNCOMPLETE_CASCADE,
};

#define IS_ENTER(k) ((k) == '\n' || (k) == '\r' || (k) == KEY_ENTER)
#define IS_BACKSPACE(k) ((k) == KEY_BACKSPACE || (k) == 127 || (k) == 8)
#define IS_ESC(k) ((k) == 27)
#define IS_PRINTABLE(k) ((k) >= 32 && (k) < 127)

static void clamp_index(int *idx, size_t n)
{
	if (n == 0) {
		*idx = 0;
		return;
	}
	if (*idx < 0)
		*idx = 0;
	if ((size_t)*idx >= n)
		*idx = (int)n - 1;
}

static void text_insert(char *buf, size_t buf_cap, size_t *cursor, char c)
{
	size_t len = strlen(buf);
	if (len + 1 >= buf_cap)
		return;
	memmove(&buf[*cursor + 1], &buf[*cursor], len - *cursor + 1);
	buf[*cursor] = c;
	(*cursor)++;
}

static void text_backspace(char *buf, size_t *cursor)
{
	if (*cursor == 0)
		return;
	memmove(&buf[*cursor - 1], &buf[*cursor], strlen(&buf[*cursor]) + 1);
	(*cursor)--;
}

static dispatch_result_t open_new_project_form(app_state_t *st)
{
	project_t resolved = {0};
	char prefill[PROJECT_NAME_MAX] = "";

	if (project_resolve_or_provisional(&resolved, NULL) == RT_SUCCESS) {
		if (resolved.id == 0)
			snprintf(prefill, sizeof(prefill), "%s", resolved.display_name);
		project_model_free(&resolved);
	}

	app_state_enter_project_form_new(st, prefill);
	return ACTION_REDRAW;
}

static dispatch_result_t dispatch_navigate_projects(int key, app_state_t *st)
{
	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(st->archived_shown_projects, &arr, &n);
	clamp_index(&st->project_sel, n);
	project_t *sel = ((size_t)st->project_sel < n) ? &arr[st->project_sel] : NULL;

	dispatch_result_t result = ACTION_NONE;

	if (key == KEY_UP) {
		if (st->project_sel > 0)
			st->project_sel--;
		result = ACTION_REDRAW;
	} else if (key == KEY_DOWN) {
		if ((size_t)(st->project_sel + 1) < n)
			st->project_sel++;
		result = ACTION_REDRAW;
	} else if (IS_ENTER(key) && sel != NULL) {
		st->current_project_id = sel->id;
		st->focus = FOCUS_TASKS;
		st->task_sel = 0;
		result = ACTION_REDRAW;
	} else if (key == 'n' || key == 'i') {
		storage_project_array_free(arr, n);
		return open_new_project_form(st);
	} else if (key == 'r' && sel != NULL && !sel->builtin) {
		app_state_enter_project_form_rename(st, sel->id, sel->display_name);
		result = ACTION_REDRAW;
	} else if (key == 'A') {
		st->archived_shown_projects = !st->archived_shown_projects;
		st->project_sel = 0;
		result = ACTION_REDRAW;
	} else if (key == 'a' && sel != NULL) {
		if (st->archived_shown_projects) {
			if (sel->archived)
				project_restore(sel->id);
		} else if (!sel->builtin) {
			project_archive(sel->id);
		}
		result = ACTION_REDRAW;
	} else if (key == 'd' && sel != NULL) {
		char msg[400];
		int action;
		if (sel->builtin) {
			snprintf(msg, sizeof(msg), "Delete all tasks in %s (keep %s)? y/n/Y",
				sel->display_name, sel->display_name);
			action = CONFIRM_ACTION_CLEAR_PROJECT_TASKS;
		} else {
			snprintf(msg, sizeof(msg), "Delete project \"%s\" and all its tasks? y/n/Y",
				sel->display_name);
			action = CONFIRM_ACTION_DELETE_PROJECT_CASCADE;
		}
		if (confirm_state_should_prompt(&st->confirm, CONFIRM_CAT_PROJECTS))
			app_state_enter_confirm(st, CONFIRM_CAT_PROJECTS, msg, action, sel->id);
		else
			project_delete_or_clear(sel->id);
		result = ACTION_REDRAW;
	}

	storage_project_array_free(arr, n);
	return result;
}

static dispatch_result_t dispatch_navigate_tasks(int key, app_state_t *st)
{
	if (st->current_project_id == 0)
		return ACTION_NONE;

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(st->current_project_id, st->archived_shown_tasks, &arr, &n);
	clamp_index(&st->task_sel, n);
	task_t *sel = ((size_t)st->task_sel < n) ? &arr[st->task_sel] : NULL;

	dispatch_result_t result = ACTION_NONE;

	if (key == KEY_UP) {
		if (st->task_sel > 0)
			st->task_sel--;
		result = ACTION_REDRAW;
	} else if (key == KEY_DOWN) {
		if ((size_t)(st->task_sel + 1) < n)
			st->task_sel++;
		result = ACTION_REDRAW;
	} else if (IS_ENTER(key) && sel != NULL) {
		char parent_buf[TASK_TITLE_MAX] = "";
		const char *parent_title = NULL;
		if (sel->parent_id != 0) {
			task_t parent;
			if (storage_task_get(sel->parent_id, &parent) == RT_SUCCESS) {
				snprintf(parent_buf, sizeof(parent_buf), "%s", parent.title);
				parent_title = parent_buf;
				task_model_free(&parent);
			}
		}
		app_state_enter_task_form_edit(st, sel, parent_title);
		result = ACTION_REDRAW;
	} else if (key == 'i') {
		app_state_enter_task_form_new(st, st->current_project_id);
		result = ACTION_REDRAW;
	} else if (key == 's' && sel != NULL && sel->parent_id == 0) {
		app_state_enter_task_form_new_subtask(st, st->current_project_id, sel->id, sel->title);
		result = ACTION_REDRAW;
	} else if (key == ' ' && sel != NULL) {
		bool completing = (sel->status == TASK_STATUS_OPEN);
		int subtask_count = 0;
		int rc = task_set_completed(sel->id, completing, false, &subtask_count);
		if (rc != RT_SUCCESS && subtask_count > 0) {
			char msg[400];
			snprintf(msg, sizeof(msg), "%s task \"%s\" and its %d subtask%s? y/n/Y",
				completing ? "Complete" : "Un-complete", sel->title, subtask_count,
				subtask_count == 1 ? "" : "s");
			int action = completing ? CONFIRM_ACTION_COMPLETE_CASCADE
			                        : CONFIRM_ACTION_UNCOMPLETE_CASCADE;
			if (confirm_state_should_prompt(&st->confirm, CONFIRM_CAT_TASKS))
				app_state_enter_confirm(st, CONFIRM_CAT_TASKS, msg, action, sel->id);
			else
				task_set_completed(sel->id, completing, true, &subtask_count);
		}
		result = ACTION_REDRAW;
	} else if ((key == '1' || key == '2' || key == '3') && sel != NULL) {
		priority_t p = (key == '1') ? PRIORITY_P1 : (key == '2') ? PRIORITY_P2 : PRIORITY_P3;
		task_set_priority(sel->id, p);
		result = ACTION_REDRAW;
	} else if (key == 'o' && sel != NULL) {
		app_state_enter_reorder(st, sel->id, sel->parent_id);
		result = ACTION_REDRAW;
	} else if (key == 'A') {
		st->archived_shown_tasks = !st->archived_shown_tasks;
		st->task_sel = 0;
		result = ACTION_REDRAW;
	} else if (key == 'a') {
		if (st->archived_shown_tasks) {
			if (sel != NULL && sel->archived)
				task_restore(sel->id);
		} else {
			int count = 0;
			task_archive_completed(st->current_project_id, &count, false);
			if (count > 0) {
				char msg[400];
				snprintf(msg, sizeof(msg), "Archive %d completed tasks? y/n/Y", count);
				if (confirm_state_should_prompt(&st->confirm, CONFIRM_CAT_TASKS))
					app_state_enter_confirm(st, CONFIRM_CAT_TASKS, msg,
						CONFIRM_ACTION_ARCHIVE_COMPLETED, st->current_project_id);
				else
					task_archive_completed(st->current_project_id, &count, true);
			}
		}
		result = ACTION_REDRAW;
	} else if (key == 'd' && sel != NULL) {
		char msg[400];
		bool has_subtasks = (sel->parent_id == 0 && storage_task_has_subtasks(sel->id) > 0);
		if (has_subtasks)
			snprintf(msg, sizeof(msg), "Delete task \"%s\" and its subtasks? y/n/Y", sel->title);
		else
			snprintf(msg, sizeof(msg), "Delete task \"%s\"? y/n/Y", sel->title);
		if (confirm_state_should_prompt(&st->confirm, CONFIRM_CAT_TASKS))
			app_state_enter_confirm(st, CONFIRM_CAT_TASKS, msg, CONFIRM_ACTION_DELETE_TASK, sel->id);
		else
			task_delete(sel->id);
		result = ACTION_REDRAW;
	}

	storage_task_array_free(arr, n);
	return result;
}

static dispatch_result_t dispatch_navigate_notes(int key, app_state_t *st)
{
	/* Derived live from current_project_id + task_sel on every keypress, not
	   cached, so it can never go stale relative to the Tasks list. */
	task_t t;
	bool have_task = (st->current_project_id != 0
		&& task_get_visible_row(st->current_project_id, st->archived_shown_tasks,
			st->task_sel, &t) == RT_SUCCESS);

	dispatch_result_t result = ACTION_NONE;

	if (key == 'i') {
		result = have_task ? ACTION_EDIT_NOTES : ACTION_NONE;
	} else if (key == 'd' && have_task) {
		char msg[400];
		snprintf(msg, sizeof(msg), "Clear notes for \"%s\"? y/n/Y", t.title);
		if (confirm_state_should_prompt(&st->confirm, CONFIRM_CAT_NOTES))
			app_state_enter_confirm(st, CONFIRM_CAT_NOTES, msg, CONFIRM_ACTION_CLEAR_NOTES, t.id);
		else
			task_clear_notes(t.id);
		result = ACTION_REDRAW;
	}

	if (have_task)
		task_model_free(&t);
	return result;
}

static dispatch_result_t dispatch_navigate(int key, app_state_t *st, layout_tier_t tier)
{
	if (key == 'q')
		return ACTION_QUIT;
	if (key == '?') {
		app_state_toggle_help(st);
		return ACTION_REDRAW;
	}
	if (key == KEY_LEFT) {
		app_state_focus_left(st, tier);
		return ACTION_REDRAW;
	}
	if (key == KEY_RIGHT) {
		app_state_focus_right(st, tier);
		return ACTION_REDRAW;
	}
	if (key == 'p') {
		app_state_enter_project_switcher(st);
		return ACTION_REDRAW;
	}
	if (key == 'n')
		return open_new_project_form(st);

	switch (st->focus) {
	case FOCUS_PROJECTS: return dispatch_navigate_projects(key, st);
	case FOCUS_TASKS:    return dispatch_navigate_tasks(key, st);
	case FOCUS_NOTES:    return dispatch_navigate_notes(key, st);
	default:             return ACTION_NONE;
	}
}

static dispatch_result_t dispatch_task_form(int key, app_state_t *st)
{
	task_form_state_t *f = &st->task_form;

	if (IS_ESC(key)) {
		app_state_exit_form(st);
		return ACTION_REDRAW;
	}
	if (IS_ENTER(key)) {
		f->error[0] = '\0';
		int rc;
		if (f->is_new) {
			task_t out;
			rc = task_create(f->project_id, f->parent_id, f->name, f->priority, &out);
			if (rc == RT_SUCCESS)
				task_model_free(&out);
		} else {
			rc = task_update_fields(f->task_id, f->name, NULL);
			if (rc == RT_SUCCESS)
				rc = task_set_priority(f->task_id, f->priority);
		}
		if (rc == RT_SUCCESS)
			app_state_exit_form(st);
		else
			snprintf(f->error, sizeof(f->error), "Save failed; check the task title.");
		return ACTION_REDRAW;
	}
	if (key == 9) { /* Tab: cycles Name -> Priority -> Name */
		f->field = (f->field == TASK_FORM_FIELD_NAME) ? TASK_FORM_FIELD_PRIORITY
		                                               : TASK_FORM_FIELD_NAME;
		return ACTION_REDRAW;
	}
	if (key == KEY_UP) {
		f->field = TASK_FORM_FIELD_NAME;
		return ACTION_REDRAW;
	}
	if (key == KEY_DOWN) {
		f->field = TASK_FORM_FIELD_PRIORITY;
		return ACTION_REDRAW;
	}
	if (key == KEY_LEFT) {
		if (f->field == TASK_FORM_FIELD_NAME) {
			if (f->cursor > 0)
				f->cursor--;
		} else if (f->priority > PRIORITY_P1) {
			f->priority = (priority_t)((int)f->priority - 1);
		}
		return ACTION_REDRAW;
	}
	if (key == KEY_RIGHT) {
		if (f->field == TASK_FORM_FIELD_NAME) {
			if (f->cursor < strlen(f->name))
				f->cursor++;
		} else if (f->priority < PRIORITY_P3) {
			f->priority = (priority_t)((int)f->priority + 1);
		}
		return ACTION_REDRAW;
	}
	if ((key == '1' || key == '2' || key == '3') && f->field == TASK_FORM_FIELD_PRIORITY) {
		f->priority = (key == '1') ? PRIORITY_P1 : (key == '2') ? PRIORITY_P2 : PRIORITY_P3;
		return ACTION_REDRAW;
	}
	if (IS_BACKSPACE(key)) {
		if (f->field == TASK_FORM_FIELD_NAME) {
			text_backspace(f->name, &f->cursor);
			return ACTION_REDRAW;
		}
		return ACTION_NONE;
	}
	if (f->field == TASK_FORM_FIELD_NAME && IS_PRINTABLE(key)) {
		text_insert(f->name, sizeof(f->name), &f->cursor, (char)key);
		return ACTION_REDRAW;
	}
	return ACTION_NONE;
}

static dispatch_result_t dispatch_project_form(int key, app_state_t *st)
{
	project_form_state_t *f = &st->project_form;

	if (IS_ESC(key)) {
		app_state_exit_form(st);
		return ACTION_REDRAW;
	}
	if (IS_ENTER(key)) {
		f->error[0] = '\0';
		int rc;
		if (f->is_rename) {
			rc = project_rename(f->project_id, f->name);
		} else {
			project_t out;
			rc = project_create_explicit(f->name, NULL, &out);
			if (rc == RT_SUCCESS) {
				project_t *arr = NULL;
				size_t n = 0;
				if (storage_project_list(st->archived_shown_projects, &arr, &n) == RT_SUCCESS) {
					for (size_t i = 0; i < n; i++) {
						if (arr[i].id == out.id) {
							st->project_sel = (int)i;
							break;
						}
					}
					storage_project_array_free(arr, n);
				}
				st->focus = FOCUS_PROJECTS;
				project_model_free(&out);
			}
		}
		if (rc == RT_SUCCESS)
			app_state_exit_form(st);
		else
			snprintf(f->error, sizeof(f->error), "Save failed; check the project name.");
		return ACTION_REDRAW;
	}
	if (key == KEY_LEFT) {
		if (f->cursor > 0)
			f->cursor--;
		return ACTION_REDRAW;
	}
	if (key == KEY_RIGHT) {
		if (f->cursor < strlen(f->name))
			f->cursor++;
		return ACTION_REDRAW;
	}
	if (IS_BACKSPACE(key)) {
		text_backspace(f->name, &f->cursor);
		return ACTION_REDRAW;
	}
	if (IS_PRINTABLE(key)) {
		text_insert(f->name, sizeof(f->name), &f->cursor, (char)key);
		return ACTION_REDRAW;
	}
	return ACTION_NONE;
}

static dispatch_result_t dispatch_reorder(int key, app_state_t *st)
{
	if (key == KEY_UP) {
		task_reorder_step(st->reorder.task_id, -1);
		return ACTION_REDRAW;
	}
	if (key == KEY_DOWN) {
		task_reorder_step(st->reorder.task_id, 1);
		return ACTION_REDRAW;
	}
	if (IS_ENTER(key)) {
		app_state_exit_reorder(st);
		return ACTION_REDRAW;
	}
	return ACTION_NONE;
}

static dispatch_result_t dispatch_confirm(int key, app_state_t *st)
{
	if (key != 'y' && key != 'n' && key != 'Y')
		key = 'n'; /* "any other response cancels" */

	bool proceed = false;
	int action = 0;
	int64_t target_id = 0;
	app_state_confirm_answer(st, (char)key, &proceed, &action, &target_id);

	if (proceed) {
		int count;
		switch (action) {
		case CONFIRM_ACTION_DELETE_PROJECT_CASCADE:
			project_delete_or_clear(target_id);
			if (st->current_project_id == target_id)
				st->current_project_id = 0;
			break;
		case CONFIRM_ACTION_CLEAR_PROJECT_TASKS:
			project_delete_or_clear(target_id);
			break;
		case CONFIRM_ACTION_DELETE_TASK:
			task_delete(target_id);
			break;
		case CONFIRM_ACTION_CLEAR_NOTES:
			task_clear_notes(target_id);
			break;
		case CONFIRM_ACTION_ARCHIVE_COMPLETED:
			task_archive_completed(target_id, &count, true);
			break;
		case CONFIRM_ACTION_COMPLETE_CASCADE:
			task_set_completed(target_id, true, true, &count);
			break;
		case CONFIRM_ACTION_UNCOMPLETE_CASCADE:
			task_set_completed(target_id, false, true, &count);
			break;
		default:
			break;
		}
	}
	return ACTION_REDRAW;
}

static dispatch_result_t dispatch_help(int key, app_state_t *st)
{
	if (key == '?' || IS_ESC(key)) {
		app_state_toggle_help(st);
		return ACTION_REDRAW;
	}
	return ACTION_NONE;
}

static dispatch_result_t dispatch_switcher(int key, app_state_t *st)
{
	if (IS_ESC(key)) {
		app_state_exit_project_switcher(st);
		return ACTION_REDRAW;
	}

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_search(st->switcher_query, false, &arr, &n);
	clamp_index(&st->switcher_sel, n);

	dispatch_result_t result = ACTION_NONE;

	if (key == KEY_UP) {
		if (st->switcher_sel > 0)
			st->switcher_sel--;
		result = ACTION_REDRAW;
	} else if (key == KEY_DOWN) {
		if ((size_t)(st->switcher_sel + 1) < n)
			st->switcher_sel++;
		result = ACTION_REDRAW;
	} else if (IS_ENTER(key)) {
		if ((size_t)st->switcher_sel < n) {
			st->current_project_id = arr[st->switcher_sel].id;
			st->focus = FOCUS_TASKS;
			st->task_sel = 0;
		}
		app_state_exit_project_switcher(st);
		result = ACTION_REDRAW;
	} else if (IS_BACKSPACE(key)) {
		size_t len = strlen(st->switcher_query);
		if (len > 0)
			st->switcher_query[len - 1] = '\0';
		st->switcher_sel = 0;
		result = ACTION_REDRAW;
	} else if (IS_PRINTABLE(key)) {
		size_t len = strlen(st->switcher_query);
		text_insert(st->switcher_query, sizeof(st->switcher_query), &len, (char)key);
		st->switcher_sel = 0;
		result = ACTION_REDRAW;
	}

	storage_project_array_free(arr, n);
	return result;
}

dispatch_result_t input_dispatch_key(int key, app_state_t *st, layout_tier_t tier)
{
	if (st == NULL)
		return ACTION_NONE;

	switch (st->mode) {
	case MODE_NAVIGATE:         return dispatch_navigate(key, st, tier);
	case MODE_TASK_FORM:        return dispatch_task_form(key, st);
	case MODE_PROJECT_FORM:     return dispatch_project_form(key, st);
	case MODE_REORDER:          return dispatch_reorder(key, st);
	case MODE_CONFIRM:          return dispatch_confirm(key, st);
	case MODE_HELP:             return dispatch_help(key, st);
	case MODE_PROJECT_SWITCHER: return dispatch_switcher(key, st);
	default:                    return ACTION_NONE;
	}
}
