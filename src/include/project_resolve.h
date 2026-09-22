// lspdiag

/**
 * @file project_resolve.h
 * @brief cwd -> project resolution decision logic.
 *
 * The directory walk itself isn't a SQL concern, but "is this ancestor path
 * registered" is a single storage_project_find_by_path() call per ancestor -
 * no separate lookup-abstraction/mock layer.
 */

#ifndef __TODO_PROJECT_RESOLVE_H
#define __TODO_PROJECT_RESOLVE_H

#include <stddef.h>

#include <task_model.h>

/** @enum resolve_kind_t Outcome of resolving a directory to a project context. */
typedef enum {
	RESOLVE_HOME_TODAY,   /**< cwd is the canonical home directory itself. */
	RESOLVE_REGISTERED,   /**< cwd or an ancestor is a registered project. */
	RESOLVE_PROVISIONAL,  /**< No registered ancestor; caller may provision one. */
} resolve_kind_t;

/**
 * @brief Resolve a (cwd, home) pair to a project context.
 *
 * Walks upward from @p cwd through each ancestor directory looking for a
 * registered project root. @p cwd == @p home short-circuits to
 * RESOLVE_HOME_TODAY before any lookup and never registers home as a
 * project; the exception applies only to home itself, not its subdirectories.
 *
 * @param cwd    Canonical (realpath'd) current working directory.
 * @param home   Canonical (realpath'd) home directory.
 * @param out_kind Set to the resolution outcome.
 * @param out_project Filled with the matching project on RESOLVE_REGISTERED
 *                     or the Today built-in on RESOLVE_HOME_TODAY. May be NULL.
 * @param out_provisional_path Filled with @p cwd on RESOLVE_PROVISIONAL. May be NULL.
 * @param path_cap Capacity of @p out_provisional_path.
 * @return RT_SUCCESS or RT_ERROR.
 */
int project_resolve_cwd(const char *cwd, const char *home,
                         resolve_kind_t *out_kind, project_t *out_project,
                         char *out_provisional_path, size_t path_cap);

/**
 * @brief Resolve the real process cwd/HOME via project_resolve_cwd().
 *
 * The only part of this module not directly unit-tested (needs a real
 * filesystem); verified by running the interactive binary.
 */
int project_resolve_current(resolve_kind_t *out_kind, project_t *out_project,
                             char *out_provisional_path, size_t path_cap);

#endif //__TODO_PROJECT_RESOLVE_H
