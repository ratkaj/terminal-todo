// lspdiag

/**
 * @file project.h
 * @brief Project validation and orchestration over storage.c/project_resolve.c.
 */

#ifndef __TODO_PROJECT_H
#define __TODO_PROJECT_H

#include <stdbool.h>
#include <stdint.h>

#include <task_model.h>

int project_create_explicit(const char *display_name, const char *canonical_path,
                             project_t *out);

/**
 * @brief Resolve the current directory to a project, provisioning in memory if needed.
 *
 * On RESOLVE_PROVISIONAL, @p out is filled in-memory only (id == 0, not yet
 * persisted); the caller must use project_commit_provisional_with_task() to
 * save it, and only once its first task is successfully created.
 */
int project_resolve_or_provisional(project_t *out, bool *out_is_provisional);

/**
 * @brief Atomically persist a provisional project together with its first task.
 *
 * Single transaction: INSERT project, INSERT task. Nothing persists if
 * either fails. @p provisional must have id == 0 (not yet saved).
 */
int project_commit_provisional_with_task(project_t *provisional, const char *title,
                                          priority_t prio, task_t *out_task);

int project_rename(int64_t id, const char *new_display_name);

/** @return RT_ERROR if the project is a built-in (Today/This Week/Inbox). */
int project_archive(int64_t id);

int project_restore(int64_t id);

/**
 * @brief Delete a regular project (cascading), or clear a built-in's tasks.
 *
 * Built-ins (Today/This Week/Inbox) are never deleted, only cleared.
 */
int project_delete_or_clear(int64_t id);

/**
 * @brief Find a project's row index within storage_project_list()'s ordering.
 *
 * Used to keep the Projects pane's selection (project_sel, an index) in sync
 * with whichever project is actually open/being switched to, e.g. after
 * initial launch-directory resolution or a project-switcher selection.
 *
 * @return The index, or -1 if not found (or on error).
 */
int project_find_index(bool include_archived, int64_t project_id);

#endif //__TODO_PROJECT_H
