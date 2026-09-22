// lspdiag

/**
 * @file input_dispatch.h
 * @brief Key -> domain action dispatch. No ncurses function calls (wgetch,
 * initscr, ...) - it receives an already-obtained key and never blocks on
 * terminal I/O itself, which is what keeps it testable without a terminal.
 *
 * It does include <curses.h> for the KEY_* integer constants (matching
 * wgetch()'s return value), but references no ncurses function or global.
 */

#ifndef __TODO_INPUT_DISPATCH_H
#define __TODO_INPUT_DISPATCH_H

#include <ui_state.h>

/** @enum dispatch_result_t What app_main.c should do after a dispatched key. */
typedef enum {
	ACTION_NONE,        /**< Nothing changed; no redraw needed. */
	ACTION_REDRAW,      /**< State changed; redraw. */
	ACTION_EDIT_NOTES,  /**< Hand off to notes_editor_edit() synchronously, then save and redraw. */
	ACTION_COPY_NOTES,  /**< Hand off to notes_editor_copy_clipboard() for the selected task's notes. */
	ACTION_QUIT,        /**< Exit the event loop. */
} dispatch_result_t;

/**
 * @brief Translate one raw key into a domain action, applying it immediately.
 *
 * Branches first on st->mode: MODE_NAVIGATE routes by st->focus; every other
 * mode is a modal overlay that owns all input without touching st->focus,
 * which is what keeps e.g. Left/Right inside an open form from also moving
 * the main-window pane focus.
 */
dispatch_result_t input_dispatch_key(int key, app_state_t *st, layout_tier_t tier);

#endif //__TODO_INPUT_DISPATCH_H
