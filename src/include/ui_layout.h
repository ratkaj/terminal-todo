// lspdiag

/**
 * @file ui_layout.h
 * @brief Pure responsive layout geometry. No ncurses calls - testable by
 * feeding plain (rows, cols) pairs.
 */

#ifndef __TODO_UI_LAYOUT_H
#define __TODO_UI_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

/** @enum layout_tier_t Responsive presentation tier, driven by terminal width. */
typedef enum {
	LAYOUT_WIDE,
	LAYOUT_COMPACT,
	LAYOUT_MINIMAL,
} layout_tier_t;

/** @enum pane_focus_t Which of the three logical panes has keyboard focus. */
typedef enum {
	FOCUS_PROJECTS,
	FOCUS_TASKS,
	FOCUS_NOTES,
} pane_focus_t;

/** @struct rect_t A screen rectangle in (row, col) terminal cells. A zero w/h means hidden. */
typedef struct {
	int y, x, h, w;
} rect_t;

/** @struct layout_geom_t Computed pane/footer rectangles for one frame. */
typedef struct {
	rect_t projects, tasks, notes, footer;
	bool projects_visible, notes_visible;
} layout_geom_t;

/**
 * @brief Decide the responsive tier for a terminal size.
 *
 * Driven by width: Wide shows all three panes side by side; Compact shows
 * two of the three (Tasks plus whichever of Projects/Notes matches focus);
 * Minimal shows one pane at a time.
 */
layout_tier_t ui_layout_tier(int rows, int cols);

/**
 * @brief Compute pane and footer rectangles for one frame.
 *
 * @param focus Which pane has focus; used by Compact (picks the Tasks+X
 *              pair to show) and Minimal (picks the single pane to show).
 *              Ignored by Wide, which always shows all three.
 */
void ui_layout_compute(int rows, int cols, layout_tier_t tier,
                        pane_focus_t focus, layout_geom_t *out);

/** @struct hotkey_entry_t One footer/help-overlay hotkey label, e.g. {"i", "Insert"}. */
typedef struct {
	const char *key;
	const char *label;
} hotkey_entry_t;

/** @brief How many footer rows (1 or 2, or 0 if entries is empty) are needed at this width. */
int ui_layout_footer_height(const hotkey_entry_t *entries, size_t n, int width);

/**
 * @brief Compute per-column widths for aligned footer/help-overlay rows.
 *
 * Entries fill row 1 first (its share is ceil(n/2) when two rows are
 * needed), then row 2; each column's width is the longest entry occupying
 * that column position in either row, so both rows align vertically.
 */
void ui_layout_footer_columns(const hotkey_entry_t *entries, size_t n, int width,
                               int *out_col_widths, size_t max_cols,
                               size_t *out_ncols, size_t *out_nrows);

#endif //__TODO_UI_LAYOUT_H
