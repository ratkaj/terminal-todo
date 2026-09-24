// lspdiag

/**
 * @file report.h
 * @brief Plain-text reports of tasks completed in a calendar period.
 *
 * Pure text rendering over storage.c - no ncurses, no file output. Like
 * export.h, app_main.c hands the result to notes_editor_view(). Periods use
 * local time, and weeks start on Monday.
 */

#ifndef __TODO_REPORT_H
#define __TODO_REPORT_H

#include <time.h>

/** @enum report_period_t The periods offered in the Generate report popup, in menu order. */
typedef enum {
	REPORT_THIS_WEEK,
	REPORT_LAST_WEEK,
	REPORT_THIS_MONTH,
	REPORT_LAST_MONTH,
	REPORT_PERIOD_COUNT,
} report_period_t;

/** @brief Menu label, e.g. "This week". */
const char *report_period_label(report_period_t period);

/**
 * @brief The half-open range [start, end) a period covers, in local time.
 *
 * "This week/month" runs from the period's first midnight up to and
 * including @p now; "last week/month" is the whole previous period.
 */
void report_period_range(report_period_t period, time_t now, time_t *out_start, time_t *out_end);

/**
 * @brief Render the tasks completed in @p period, grouped by project.
 *
 * @param now      Reference time for the period and the "Generated:" line.
 * @param out_text Heap-allocated result; caller must free() it.
 * @return RT_SUCCESS or RT_ERROR (invalid period, storage or memory failure).
 */
int report_completed_text(report_period_t period, time_t now, char **out_text);

#endif //__TODO_REPORT_H
