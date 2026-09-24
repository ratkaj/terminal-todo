// lspdiag

#include <stdio.h>
#include <string.h>

#include <ui_state.h>

void app_state_init(app_state_t *st)
{
	if (st == NULL)
		return;
	memset(st, 0, sizeof(*st));
	st->mode = MODE_NAVIGATE;
	st->focus = FOCUS_TASKS;
	confirm_state_init(&st->confirm);
}

void app_state_focus_left(app_state_t *st, layout_tier_t tier)
{
	(void)tier; /* navigation order/bounds are the same at every tier */
	if (st == NULL || st->mode != MODE_NAVIGATE)
		return;
	if (st->focus == FOCUS_TASKS)
		st->focus = FOCUS_PROJECTS;
	else if (st->focus == FOCUS_NOTES)
		st->focus = FOCUS_TASKS;
	/* FOCUS_PROJECTS: already leftmost, no wrap. */
}

void app_state_focus_right(app_state_t *st, layout_tier_t tier)
{
	(void)tier;
	if (st == NULL || st->mode != MODE_NAVIGATE)
		return;
	if (st->focus == FOCUS_PROJECTS)
		st->focus = FOCUS_TASKS;
	else if (st->focus == FOCUS_TASKS)
		st->focus = FOCUS_NOTES;
	/* FOCUS_NOTES: already rightmost, no wrap. */
}

void app_state_enter_task_form_new(app_state_t *st, int64_t project_id)
{
	if (st == NULL)
		return;
	memset(&st->task_form, 0, sizeof(st->task_form));
	st->task_form.is_new = true;
	/* provisional_active alone isn't enough: it stays true (the provisional
	   project isn't dropped) even while browsing a different, already-real
	   project such as Inbox, since navigating the Projects pane no longer
	   gets stuck on the provisional row. Only commit provisional+task
	   together when the provisional row is the one actually being viewed. */
	st->task_form.is_provisional = st->provisional_active && project_id == 0;
	st->task_form.project_id = project_id;
	st->task_form.priority = PRIORITY_P3;
	st->task_form.field = TASK_FORM_FIELD_NAME;
	st->mode = MODE_TASK_FORM;
}

void app_state_enter_task_form_new_subtask(app_state_t *st, int64_t project_id,
	int64_t parent_id, const char *parent_title)
{
	if (st == NULL)
		return;
	memset(&st->task_form, 0, sizeof(st->task_form));
	st->task_form.is_new = true;
	st->task_form.is_subtask = true;
	st->task_form.project_id = project_id;
	st->task_form.parent_id = parent_id;
	if (parent_title != NULL)
		snprintf(st->task_form.parent_title, sizeof(st->task_form.parent_title),
			"%s", parent_title);
	st->task_form.priority = PRIORITY_P3;
	st->task_form.field = TASK_FORM_FIELD_NAME;
	st->mode = MODE_TASK_FORM;
}

void app_state_enter_task_form_edit(app_state_t *st, const task_t *t, const char *parent_title)
{
	if (st == NULL || t == NULL)
		return;
	memset(&st->task_form, 0, sizeof(st->task_form));
	st->task_form.is_new = false;
	st->task_form.is_subtask = (t->parent_id != 0);
	st->task_form.task_id = t->id;
	st->task_form.project_id = t->project_id;
	st->task_form.parent_id = t->parent_id;
	if (parent_title != NULL)
		snprintf(st->task_form.parent_title, sizeof(st->task_form.parent_title),
			"%s", parent_title);
	snprintf(st->task_form.name, sizeof(st->task_form.name), "%s", t->title);
	st->task_form.cursor = strlen(st->task_form.name);
	st->task_form.priority = t->priority;
	st->task_form.field = TASK_FORM_FIELD_NAME;
	st->mode = MODE_TASK_FORM;
}

void app_state_enter_project_form_new(app_state_t *st, const char *prefill_name)
{
	if (st == NULL)
		return;
	memset(&st->project_form, 0, sizeof(st->project_form));
	if (prefill_name != NULL) {
		snprintf(st->project_form.name, sizeof(st->project_form.name), "%s", prefill_name);
		st->project_form.cursor = strlen(st->project_form.name);
	}
	st->mode = MODE_PROJECT_FORM;
}

void app_state_enter_project_form_rename(app_state_t *st, int64_t project_id,
	const char *current_name)
{
	if (st == NULL)
		return;
	memset(&st->project_form, 0, sizeof(st->project_form));
	st->project_form.is_rename = true;
	st->project_form.project_id = project_id;
	if (current_name != NULL) {
		snprintf(st->project_form.name, sizeof(st->project_form.name), "%s", current_name);
		st->project_form.cursor = strlen(st->project_form.name);
	}
	st->mode = MODE_PROJECT_FORM;
}

void app_state_exit_form(app_state_t *st)
{
	if (st == NULL)
		return;
	st->mode = MODE_NAVIGATE;
}

void app_state_enter_confirm(app_state_t *st, confirm_category_t cat,
	const char *message, int action, int64_t target_id)
{
	if (st == NULL)
		return;
	memset(&st->pending_confirm, 0, sizeof(st->pending_confirm));
	st->pending_confirm.category = cat;
	if (message != NULL)
		snprintf(st->pending_confirm.message, sizeof(st->pending_confirm.message), "%s", message);
	st->pending_confirm.action = action;
	st->pending_confirm.target_id = target_id;
	st->mode = MODE_CONFIRM;
}

void app_state_confirm_answer(app_state_t *st, char answer, bool *out_proceed,
	int *out_action, int64_t *out_target_id)
{
	if (st == NULL)
		return;
	bool proceed = false;
	confirm_state_apply_answer(&st->confirm, st->pending_confirm.category, answer, &proceed);
	if (out_proceed != NULL)
		*out_proceed = proceed;
	if (out_action != NULL)
		*out_action = st->pending_confirm.action;
	if (out_target_id != NULL)
		*out_target_id = st->pending_confirm.target_id;
	st->mode = MODE_NAVIGATE;
}

void app_state_enter_reorder(app_state_t *st, int64_t task_id, int64_t parent_id)
{
	if (st == NULL)
		return;
	st->reorder.task_id = task_id;
	st->reorder.parent_id = parent_id;
	st->mode = MODE_REORDER;
}

void app_state_exit_reorder(app_state_t *st)
{
	if (st == NULL)
		return;
	st->mode = MODE_NAVIGATE;
}

void app_state_enter_task_move(app_state_t *st, int64_t task_id, const char *title)
{
	if (st == NULL)
		return;
	st->task_move.task_id = task_id;
	snprintf(st->task_move.title, sizeof(st->task_move.title), "%s", title ? title : "");
	st->task_move.sel = 0;
	st->mode = MODE_TASK_MOVE;
}

void app_state_exit_task_move(app_state_t *st)
{
	if (st == NULL)
		return;
	st->mode = MODE_NAVIGATE;
}

void app_state_toggle_help(app_state_t *st)
{
	if (st == NULL)
		return;
	st->mode = (st->mode == MODE_HELP) ? MODE_NAVIGATE : MODE_HELP;
	st->help_scroll = 0;
}

void app_state_enter_project_switcher(app_state_t *st)
{
	if (st == NULL)
		return;
	st->switcher_query[0] = '\0';
	st->switcher_sel = 0;
	st->mode = MODE_PROJECT_SWITCHER;
}

void app_state_exit_project_switcher(app_state_t *st)
{
	if (st == NULL)
		return;
	st->mode = MODE_NAVIGATE;
}
