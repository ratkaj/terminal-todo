// lspdiag

/**
 * @file confirm.h
 * @brief Session-only, per-category confirmation-suppression bookkeeping.
 *
 * Pure state, no I/O. The UI layer asks confirm_state_should_prompt(), shows
 * a prompt if needed, and only then calls the mutating domain function.
 */

#ifndef __TODO_CONFIRM_H
#define __TODO_CONFIRM_H

#include <stdbool.h>

/**
 * @enum confirm_category_t
 * @brief Independent confirmation-suppression categories.
 */
typedef enum {
	CONFIRM_CAT_PROJECTS = 0,
	CONFIRM_CAT_TASKS,
	CONFIRM_CAT_NOTES,
	CONFIRM_CAT_COUNT,
} confirm_category_t;

/** @struct confirm_state_t Per-category suppression flags for one session. */
typedef struct {
	bool suppressed[CONFIRM_CAT_COUNT];
} confirm_state_t;

/** @brief Reset all categories to "should prompt" (call once at app start). */
void confirm_state_init(confirm_state_t *cs);

/** @brief Whether a confirmation prompt should be shown for @p cat. */
bool confirm_state_should_prompt(const confirm_state_t *cs, confirm_category_t cat);

/**
 * @brief Apply a y/n/Y answer for @p cat.
 *
 * 'y' or 'Y' proceeds; 'Y' additionally suppresses further prompts for this
 * category for the rest of the session. Any other answer cancels.
 *
 * @param out_proceed Set to true if the action should be performed.
 */
void confirm_state_apply_answer(confirm_state_t *cs, confirm_category_t cat,
                                 char answer, bool *out_proceed);

#endif //__TODO_CONFIRM_H
