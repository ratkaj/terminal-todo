// lspdiag

/**
 * @file fuzzy.h
 * @brief Case-insensitive fuzzy matching for task search.
 *
 * Pure functions with no ncurses or SQLite. storage.c registers them as the
 * SQL functions todo_fuzzy() and todo_words_in(), so search filtering and
 * ordering stay in one query. Case folding is utf8_fold(), so it follows
 * LC_CTYPE like the project switcher.
 */

#ifndef __TODO_FUZZY_H
#define __TODO_FUZZY_H

#include <stdbool.h>

/**
 * @brief Score @p text against @p query, fzf style.
 *
 * The query is split on spaces; every word must appear in @p text as an
 * in-order subsequence (gaps allowed), and the words may match in any order.
 * Consecutive letters, letters at the start of a word, and a match at the
 * very start of @p text score higher; gaps between letters cost a little.
 *
 * @return -1 when some word does not match, otherwise a score >= 0 where
 *         higher is better. A query with no words matches with score 0.
 */
int fuzzy_score(const char *text, const char *query);

/**
 * @brief Whether every space-separated word of @p query appears in @p text
 *        as a contiguous, case-insensitive substring (in any order).
 *
 * Used for the weaker notes match: subsequence matching on long notes would
 * match nearly any short query. NULL @p text never matches; a query with no
 * words always does.
 */
bool fuzzy_words_substring(const char *text, const char *query);

#endif //__TODO_FUZZY_H
