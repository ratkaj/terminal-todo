// lspdiag

/**
 * @file storage.h
 * @brief SQLite persistence gateway. The only module that includes sqlite3.h.
 *
 * Ordering, grouping, filtering, and cascading are expressed as SQL here
 * (ORDER BY, correlated subqueries, ON DELETE CASCADE, a nesting-invariant
 * trigger) rather than re-implemented as C algorithms. Callers in task.c/
 * project.c are thin validation/orchestration only.
 */

#ifndef __TODO_STORAGE_H
#define __TODO_STORAGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <task_model.h>

/**
 * @brief Open (creating/migrating as needed) the SQLite database.
 *
 * Creates the parent directory if missing, enables foreign key enforcement,
 * creates the schema if absent, and idempotently seeds the Today/This Week/
 * Inbox built-in projects.
 *
 * @param db_path Path to the database file, or NULL for the default
 *                (~/.local/share/todo/todo.db).
 * @return RT_SUCCESS or RT_ERROR.
 */
int storage_open(const char *db_path);

/** @brief Close the database opened by storage_open(). Safe to call twice. */
void storage_close(void);

int storage_begin(void);
int storage_commit(void);
int storage_rollback(void);

/* --- projects --- */

int storage_project_insert(const project_t *p, int64_t *out_id);
int storage_project_update(const project_t *p);
int storage_project_get(int64_t id, project_t *out);
/** @return RT_SUCCESS if found, RT_ERROR if not found or on error. */
int storage_project_find_by_path(const char *canonical_path, project_t *out);
int storage_project_list(bool include_archived, project_t **out_arr, size_t *out_n);
int storage_project_search(const char *query, bool include_archived,
                            project_t **out_arr, size_t *out_n);
/** @return top-level, non-archived task count, or -1 on error. */
int storage_project_task_count(int64_t project_id);
int storage_project_delete_cascade(int64_t id);
int storage_project_clear_tasks(int64_t id);
void storage_project_array_free(project_t *arr, size_t n);

/* --- tasks --- */

/** Rows are already ordered (state, priority, manual_order) and filtered by SQL. */
int storage_task_list_top_level(int64_t project_id, bool include_archived,
                                 task_t **out_arr, size_t *out_n);
int storage_task_list_subtasks(int64_t parent_id, bool include_archived,
                                task_t **out_arr, size_t *out_n);
int storage_task_get(int64_t id, task_t *out);
int storage_task_insert(int64_t project_id, int64_t parent_id, const char *title,
                         priority_t priority, task_t *out);
/** NULL title or notes means "leave that field unchanged". */
int storage_task_update_fields(int64_t id, const char *title, const char *notes);
int storage_task_set_priority(int64_t id, priority_t new_priority);
/** @return subtask count, or -1 on error. */
int storage_task_has_subtasks(int64_t id);
int storage_task_set_completed(int64_t id, bool completed, bool cascade_subtasks);
/** @return RT_ERROR both on failure and at a group boundary/edge (no-op). */
int storage_task_reorder_move(int64_t id, int direction);
int storage_task_delete_cascade(int64_t id);
int storage_task_clear_notes(int64_t id);
int storage_task_archive_completed(int64_t project_id, int *out_count, bool apply);
int storage_task_restore(int64_t id);
void storage_task_array_free(task_t *arr, size_t n);

#endif //__TODO_STORAGE_H
