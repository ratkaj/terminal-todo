// lspdiag

/**
 * @file ui_draw.h
 * @brief ncurses rendering. Queries storage directly (like input_dispatch.c
 * does) rather than requiring pre-fetched snapshots; app_main.c stays thin.
 *
 * Not unit-tested; verified manually against the window templates.
 */

#ifndef __TODO_UI_DRAW_H
#define __TODO_UI_DRAW_H

#include <ui_state.h>

/** @brief setlocale/initscr/keypad/color bring-up. @return RT_SUCCESS or RT_ERROR. */
int ui_draw_init(void);

/** @brief endwin(). Safe to call after ui_draw_init() failed partway. */
void ui_draw_shutdown(void);

/** @brief Recompute layout from the current terminal size and draw one frame. */
void ui_draw_frame(const app_state_t *st);

#endif //__TODO_UI_DRAW_H
