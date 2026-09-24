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
 * @param footer_rows Rows the footer's entries need at this width (see
 *              ui_layout_footer_height()). The footer gets exactly that many
 *              rows, or none when it needs more than 3 rows, the window is
 *              too short to spare them, or the tier is Minimal.
 */
void ui_layout_compute(int rows, int cols, layout_tier_t tier,
                        pane_focus_t focus, int footer_rows, layout_geom_t *out);

/** @struct hotkey_entry_t One footer/help-overlay hotkey label, e.g. {"i", "Insert"}. */
typedef struct {
	const char *key;
	const char *label;
} hotkey_entry_t;

/**
 * @brief How many rows an aligned footer needs at this width (0 if entries is empty).
 *
 * Uses as many columns as fit (see ui_layout_footer_columns()), so a wider
 * window needs fewer rows.
 */
int ui_layout_footer_height(const hotkey_entry_t *entries, size_t n, int width);

/**
 * @brief Compute per-column widths for aligned footer rows.
 *
 * Picks the most columns (at most @p max_cols) that fit @p width and fills
 * them row by row: entry i goes to row i / ncols, column i % ncols. Each
 * column's width is the longest entry in that column, so rows align
 * vertically.
 */
void ui_layout_footer_columns(const hotkey_entry_t *entries, size_t n, int width,
                               int *out_col_widths, size_t max_cols,
                               size_t *out_ncols, size_t *out_nrows);

#endif //__TODO_UI_LAYOUT_H
