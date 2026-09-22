// lspdiag

/**
 * @file task_model.h
 * @brief Shared plain-data types for tasks and projects.
 *
 * Pure struct/enum definitions and trivial validation only. No storage or
 * ncurses dependency - see storage.h for persistence and task.h/project.h
 * for validated CRUD.
 */

#ifndef __TODO_TASK_MODEL_H
#define __TODO_TASK_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/** Maximum stored task title length, including the NUL terminator. */
#define TASK_TITLE_MAX 256

/** Maximum stored project display-name length, including the NUL terminator. */
#define PROJECT_NAME_MAX 128

/**
 * @enum priority_t
 * @brief Task priority. Lower numeric value is higher priority.
 */
typedef enum {
	PRIORITY_P1 = 1,
	PRIORITY_P2 = 2,
	PRIORITY_P3 = 3,
} priority_t;

/**
 * @enum task_status_t
 * @brief Whether a task has been completed.
 */
typedef enum {
	TASK_STATUS_OPEN = 0,
	TASK_STATUS_COMPLETED = 1,
} task_status_t;

/**
 * @enum task_state_t
 * @brief Display-grouping state derived from status + archived.
 *
 * Always computed by storage.c's queries (a single source of truth); never
 * re-derived independently elsewhere.
 */
typedef enum {
	TASK_STATE_ACTIVE = 0,
	TASK_STATE_COMPLETED = 1,
	TASK_STATE_ARCHIVED = 2,
} task_state_t;

/**
 * @struct task_t
 * @brief A task or subtask row.
 */
typedef struct {
	int64_t id;
	int64_t project_id;
	int64_t parent_id;      /**< 0 = top-level (no parent). */
	char    title[TASK_TITLE_MAX];
	char   *notes;          /**< Heap-owned, may be NULL. */
	task_status_t status;
	priority_t priority;
	long    manual_order;
	bool    archived;
	task_state_t state;     /**< Computed by storage.c. */
	time_t  created_at;
	time_t  completed_at;   /**< 0 if never completed. */
} task_t;

/**
 * @struct project_t
 * @brief A project row (directory-backed, named-without-directory, or built-in).
 */
typedef struct {
	int64_t id;
	char    display_name[PROJECT_NAME_MAX];
	char   *canonical_path; /**< Heap-owned; NULL = named-without-directory. */
	bool    archived;
	bool    builtin;        /**< Today / This Week / Inbox. */
} project_t;

/**
 * @brief Validate a raw integer as one of PRIORITY_P1/P2/P3.
 *
 * @param v Candidate priority value.
 * @return RT_SUCCESS if valid, RT_ERROR otherwise.
 */
int task_model_validate_priority(int v);

/**
 * @brief Free a task_t's heap-owned members.
 *
 * Does not free @p t itself. Safe to call with @p t == NULL.
 */
void task_model_free(task_t *t);

/**
 * @brief Free a project_t's heap-owned members.
 *
 * Does not free @p p itself. Safe to call with @p p == NULL.
 */
void project_model_free(project_t *p);

#endif //__TODO_TASK_MODEL_H
