# ncurses UI Implementation Notes

[UI specification](../ui.md) · [Development requirements](../development.md) · [Main-window template](../templates/template-fullsize-main-window.md)

These notes record ncurses implementation guidance, followed by the app's actual `ui_draw.c`/`app_main.c`/`notes_editor.c`. Product behavior is defined in the UI specification; treat this file as the "how" and `ui.md` as the "what" when the two need reconciling.

## Character width and borders

Use the wide-character build (`ncursesw`) and initialize the locale before starting curses. Measure and truncate text by terminal display cells, not UTF-8 bytes or character count. Account for combining and double-width characters when aligning task titles, notes, and hotkey columns.

Use `WACS_*` line-drawing symbols for borders and junctions. Their appearance depends on terminal capabilities and locale; keep a simple fallback for environments without suitable glyphs. See [wide-character rendering](https://invisible-island.net/ncurses/man/curs_add_wch.3x.html).

## Priority colors

Check color support and use `use_default_colors()` where supported. Configure red and yellow foregrounds against the default background; P3 uses default foreground and background. Check initialization results and retain priority labels when color is unavailable. Exact shades come from the terminal palette, so inspect readability with both light and dark terminal themes. See [default colors](https://invisible-island.net/ncurses/man/default_colors.3x.html) and [color support](https://invisible-island.net/ncurses/man/curs_color.3x.html).

## Layout and resizing

The current full-size template is 128 columns by 31 rows, not a minimum supported terminal size. It omits a separate application/project title row; the current project appears in the Tasks heading. On `KEY_RESIZE`, recalculate pane geometry and available content space, including the one- or two-line hotkey pane. Preserve selection and scroll positions where possible, clamping them to valid ranges after resizing. At limited height, hide the footer and make `?` open the centered Help overlay.

Ncurses cannot choose the application's responsive layout. Follow approved templates and the UI specification; smaller-window designs still need their own templates. See [ncurses resize guidance](https://invisible-island.net/ncurses/ncurses-intro.html).

## Shared separators and redraws

Give each shared border one drawing owner. A shared frame with pane interiors is a suitable approach; independently boxing adjacent panes can produce doubled separators. The exact window structure remains an implementation choice.

Stage pane updates with `wnoutrefresh()` and finish with one `doupdate()` per frame to avoid unnecessary intermediate redraws. See [refresh operations](https://invisible-island.net/ncurses/man/curs_refresh.3x.html).

## Notes and focus

Notes editing hands off to the user's `$EDITOR` (falling back to `vi`) against a temporary file rather than an in-app text widget; see `notes_editor.c`. The Notes pane itself still implements its own word wrapping for display (both the task title and its notes body), since ncurses' automatic character wrapping does not provide word wrapping. Keep pane focus distinct from the selected project and selected task.

Implement the accepted [pane-focus indicator](../ui.md#pane-focus) with `A_REVERSE` (or its wide-character equivalent) on the active heading and its single-space padding only. Restore attributes before drawing the remainder of the header so reversal does not leak into borders or other panes. Preserve task priority colors.

Dispatch `i` according to the focused pane as specified in the [keyboard rules](../ui.md#keyboard-operation); on the Notes pane, Enter and `n` (from Tasks or Notes) trigger the same `$EDITOR` hand-off. Do not enter notes editing without a selected task. The hand-off suspends curses (`def_prog_mode`/`endwin`) for the duration of the external editor process and resumes it (`reset_prog_mode`/`doupdate`) afterward; nothing else should assume curses stays active across that call.

Preserve case when dispatching archive keys: lowercase `a` performs the contextual archive/restore action and uppercase `A` toggles archive visibility for Projects or Tasks. Route both as ordinary text in text-entry contexts. Rebuild the visible row list after either action, keep the Projects and Tasks visibility filters independent, and clamp the selection if its previous row becomes hidden.

Use the same bounded Projects/Tasks/Notes navigation order for both visible-pane focus and dedicated compact/minimal layouts. Small layouts may rearrange or simplify the pane presentation, but must preserve the same data and actions. Interpret `Esc` by the active mode: cancel a task/subtask form (including edits to existing items), or cancel unfinished project creation. Notes editing has no separate `Esc` handling of its own: it is not a mode, and control returns to normal navigation as soon as the external editor process exits.

All windows and dialogs must use aligned hotkey columns across their available footer rows. Size each column from the longest entry in that column and leave empty cells when a row lacks an action. Do not pack each row independently.

The New Project form has one editable Project name field. Prefill it from the directory basename when available, allow replacement, and after a successful Enter save select the new project immediately.

Task forms use draft values until Enter saves successfully. Escape discards the draft without changing the stored task. Route digits to Name as text and to Priority as choices according to field focus; handle `s` as a subtask command only during task-list navigation.

Enter is the form submit action from either editable field; it never serves as a separate priority-selection step. Priority changes happen through Left/Right or `1`/`2`/`3` while Priority is focused.

In task forms, route Up/Down to editable-field navigation and Left/Right to the active field's cursor or priority selection. Skip read-only parent context, and do not pass those arrows to main-pane navigation. Tab cycles forward through the editable fields and wraps to the first field.

The `>` marker is the sole selected-row indicator. Reorder mode moves the row carrying `>`; do not introduce an additional reorder cursor.

## Bottom-right cell

Character-writing functions can draw the bottom-right cell and still return `ERR` when the cursor cannot advance with scrolling disabled. Handle this specific boundary deliberately when drawing the outer frame; do not ignore rendering errors generally or enable scrolling merely to suppress this result. See [cursor advancement and wrapping](https://invisible-island.net/ncurses/man/curs_add_wch.3x.html).

## Validation during implementation

Check Unicode alignment and truncation, exact-fit window boundaries, repeated shrinking and expansion, long notes, both footer heights, and color fallback. Verify that focus, selection, reorder state, and both archive-visibility filters survive redraws. Test layout calculations without initializing a terminal where possible, then inspect actual rendering in a terminal.
