// lspdiag

/**
 * @file ui_state.h
 * @brief The application mode/focus state machine. Pure - no ncurses, no storage.
 *
 * Notes editing has no mode of its own: it is a synchronous action dispatched
 * from MODE_NAVIGATE and handled by app_main.c (see notes_editor.h).
 */

#ifndef __TODO_UI_STATE_H
#define __TODO_UI_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <confirm.h>
#include <task_model.h>
#include <ui_layout.h>

/** @enum app_mode_t Which modal overlay (if any) currently owns input. */
typedef enum {
	MODE_NAVIGATE,
	MODE_TASK_FORM,
	MODE_PROJECT_FORM,
	MODE_REORDER,
	MODE_CONFIRM,
	MODE_HELP,
	MODE_PROJECT_SWITCHER,
	MODE_TASK_MOVE,
} app_mode_t;

typedef enum {
	TASK_FORM_FIELD_NAME,
	TASK_FORM_FIELD_PRIORITY,
} task_form_field_t;

/** @struct task_form_state_t Draft state for the shared task/subtask create-or-edit form. */
typedef struct {
	bool is_new;
	bool is_subtask;
	bool is_provisional;                /**< If true, Enter commits provisional_project + this task together. */
	int64_t task_id;                    /**< 0 if creating. */
	int64_t project_id;
	int64_t parent_id;                  /**< 0 if top-level. */
	char parent_title[TASK_TITLE_MAX];  /**< Read-only context for subtask forms. */
	char name[TASK_TITLE_MAX];
	size_t cursor;                      /**< Cursor position within name. */
	priority_t priority;
	task_form_field_t field;
	char error[128];
} task_form_state_t;

/** @struct project_form_state_t Draft state for the one-field New Project/Rename form. */
typedef struct {
	bool is_rename;
	int64_t project_id;                 /**< 0 if creating. */
	char name[PROJECT_NAME_MAX];
	size_t cursor;
	char error[128];
} project_form_state_t;

/** @struct confirm_prompt_t A pending y/n/Y confirmation and the deferred action to run on yes. */
typedef struct {
	confirm_category_t category;
	char message[256];
	int action;        /**< Caller-defined action code, interpreted by input_dispatch.c. */
	int64_t target_id; /**< The project/task id the action applies to. */
} confirm_prompt_t;

/** @struct reorder_state_t Which task (and its peer scope) is being moved in MODE_REORDER. */
typedef struct {
	int64_t task_id;
	int64_t parent_id; /**< Peer scope: 0 = top-level list, else subtasks of this parent. */
} reorder_state_t;

/** @struct task_move_state_t The task being moved in MODE_TASK_MOVE and the highlighted destination. */
typedef struct {
	int64_t task_id;
	char title[TASK_TITLE_MAX];
	int sel;           /**< Index into storage_project_list_move_targets(). */
} task_move_state_t;

/** @struct app_state_t The full application state threaded through render/dispatch. */
typedef struct {
	app_mode_t mode;
	pane_focus_t focus;
	bool archived_shown_projects;
	bool archived_shown_tasks;
	confirm_state_t confirm;

	int64_t current_project_id;
	int project_sel;
	int project_scroll;
	/* No cached "selected task id": always derive it live from
	   current_project_id + task_sel (see task_get_visible_row()) so it can
	   never go stale relative to the list. */
	int task_sel;
	int task_scroll;

	/* The launch directory didn't match any registered project: current_
	   project_id stays 0 and this holds the not-yet-saved project (display
	   name/canonical path only) until its first task is created, at which
	   point project_commit_provisional_with_task() persists both together
	   and this is cleared. */
	bool provisional_active;
	project_t provisional_project;

	task_form_state_t task_form;
	project_form_state_t project_form;
	confirm_prompt_t pending_confirm;
	reorder_state_t reorder;
	task_move_state_t task_move;

	char switcher_query[PROJECT_NAME_MAX];
	int switcher_sel;
} app_state_t;

/** @brief Reset to MODE_NAVIGATE/FOCUS_TASKS with all suppression/filters cleared. */
void app_state_init(app_state_t *st);

/** Bounded Projects <-> Tasks <-> Notes; no-op outside MODE_NAVIGATE or at either end. */
void app_state_focus_left(app_state_t *st, layout_tier_t tier);
void app_state_focus_right(app_state_t *st, layout_tier_t tier);

void app_state_enter_task_form_new(app_state_t *st, int64_t project_id);
void app_state_enter_task_form_new_subtask(app_state_t *st, int64_t project_id,
                                            int64_t parent_id, const char *parent_title);
void app_state_enter_task_form_edit(app_state_t *st, const task_t *t, const char *parent_title);

void app_state_enter_project_form_new(app_state_t *st, const char *prefill_name);
void app_state_enter_project_form_rename(app_state_t *st, int64_t project_id,
                                          const char *current_name);

/** Cancel/close whichever form is open and return to MODE_NAVIGATE. */
void app_state_exit_form(app_state_t *st);

void app_state_enter_confirm(app_state_t *st, confirm_category_t cat,
                              const char *message, int action, int64_t target_id);

/**
 * @brief Apply a y/n/Y answer to the pending confirmation and return to MODE_NAVIGATE.
 * @param out_proceed   Set to whether the deferred action should run.
 * @param out_action    Set to the action code passed to app_state_enter_confirm(). May be NULL.
 * @param out_target_id Set to the target id passed to app_state_enter_confirm(). May be NULL.
 */
void app_state_confirm_answer(app_state_t *st, char answer, bool *out_proceed,
                               int *out_action, int64_t *out_target_id);

void app_state_enter_reorder(app_state_t *st, int64_t task_id, int64_t parent_id);
void app_state_exit_reorder(app_state_t *st);

/** @brief Toggle MODE_HELP on/off, returning to MODE_NAVIGATE when closed. */
void app_state_toggle_help(app_state_t *st);

void app_state_enter_task_move(app_state_t *st, int64_t task_id, const char *title);
void app_state_exit_task_move(app_state_t *st);

void app_state_enter_project_switcher(app_state_t *st);
void app_state_exit_project_switcher(app_state_t *st);

#endif //__TODO_UI_STATE_H
