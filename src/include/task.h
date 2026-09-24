// lspdiag

/**
 * @file task.h
 * @brief Task/subtask validation and orchestration over storage.c.
 *
 * Thin layer: input validation plus calls into storage.c, which does the
 * actual ordering/grouping/cascading via SQL.
 */

#ifndef __TODO_TASK_H
#define __TODO_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include <task_model.h>

/**
 * @brief Create a task or subtask.
 *
 * The one-level-nesting invariant is enforced by a SQLite trigger (see
 * storage.c's schema); this function surfaces the trigger's abort as a
 * clean RT_ERROR rather than re-implementing the parent-of-parent check.
 *
 * @param parent_id 0 for a top-level task, or an existing top-level task's id.
 */
int task_create(int64_t project_id, int64_t parent_id, const char *title,
                 priority_t priority, task_t *out);

/** NULL title or notes leaves that field unchanged. */
int task_update_fields(int64_t id, const char *title, const char *notes);

int task_set_priority(int64_t id, priority_t new_priority);

/**
 * @brief Complete or uncomplete a task, cascading to subtasks if present.
 *
 * If the task has subtasks and @p confirmed_cascade is false, returns
 * RT_ERROR without changing anything; @p out_subtask_count (if non-NULL) is
 * always set to the subtask count, so the caller can tell "needs
 * confirmation" (count > 0) apart from a real failure (count < 0) and show
 * "Complete/uncomplete N subtasks?" before the user answers.
 */
int task_set_completed(int64_t id, bool completed, bool confirmed_cascade,
                        int *out_subtask_count);

int task_delete(int64_t id);
int task_clear_notes(int64_t id);

/**
 * @brief Move a top-level task, with its subtasks and notes, to another project.
 *
 * The task keeps its priority and completion state and goes to the end of
 * its group in the destination. Returns RT_ERROR without changing anything
 * for a subtask, an archived task, the same project, or a missing or
 * archived destination.
 */
int task_move_to_project(int64_t id, int64_t dest_project_id);

/** @return RT_ERROR both on failure and at a group boundary/edge (no-op). */
int task_reorder_step(int64_t id, int direction);

/**
 * @brief Archive all completed, non-archived tasks in a project.
 *
 * @param out_count Always set to the affected count, even if @p confirmed is
 *                   false, so the caller can show "Archive N completed
 *                   tasks?" before the user answers.
 */
int task_archive_completed(int64_t project_id, int *out_count, bool confirmed);

int task_restore(int64_t id);

/**
 * @brief List a project's tasks in display order: each top-level task
 * immediately followed by its own subtasks.
 *
 * Composes two already-SQL-ordered result sets (storage_task_list_top_level
 * + storage_task_list_subtasks per parent) - no sorting/grouping logic is
 * re-implemented here, only concatenation for rendering/selection purposes.
 */
int task_list_visible_rows(int64_t project_id, bool include_archived,
                            task_t **out_arr, size_t *out_n);

/**
 * @brief Fetch the visible row at @p index (see task_list_visible_rows()).
 *
 * Used to derive "the currently selected task" live from a task_sel index
 * on every call, rather than caching a task id that could go stale (e.g.
 * after creating a task without having navigated the list yet).
 *
 * @return RT_SUCCESS with *out filled, or RT_ERROR if index is out of range.
 */
int task_get_visible_row(int64_t project_id, bool include_archived,
                          int index, task_t *out);

/**
 * @brief Find a task's row index within task_list_visible_rows()'s ordering.
 *
 * Used to keep the Tasks pane's selection (task_sel, an index) pointed at a
 * specific task after an operation that can change its position, e.g. after
 * creating a task (so focus lands on it) or moving it during reorder (so the
 * `>` marker follows the task instead of staying on its old row index).
 *
 * @return The index, or -1 if not found (or on error).
 */
int task_find_visible_index(int64_t project_id, bool include_archived, int64_t task_id);

#endif //__TODO_TASK_H
