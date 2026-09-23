// lspdiag

/**
 * @file notes_editor.h
 * @brief Notes editing via the user's $EDITOR instead of an in-app text widget.
 *
 * The tmpfile read/write helpers are pure file I/O and directly unit-tested.
 * notes_editor_edit() additionally suspends/resumes curses and shells out to
 * a real editor process, so it is verified manually rather than under Unity.
 */

#ifndef __TODO_NOTES_EDITOR_H
#define __TODO_NOTES_EDITOR_H

#include <stddef.h>

/** @brief Write @p text (or an empty file if NULL) to a fresh temp file. */
int notes_editor_write_tmpfile(const char *text, char *out_path, size_t path_cap);

/** @brief Read the whole file at @p path into a heap buffer in @p out_text. */
int notes_editor_read_tmpfile(const char *path, char **out_text);

/**
 * @brief Edit @p initial_text in $EDITOR (falling back to "vi"), returning the result.
 *
 * On a non-zero editor exit, returns RT_ERROR and leaves @p out_text unset
 * (treated as "cancelled, keep existing notes"). Caller owns *out_text on
 * success and must free() it.
 */
int notes_editor_edit(const char *initial_text, char **out_text);

/**
 * @brief Show read-only @p text in $EDITOR (falling back to "vi").
 *
 * Writes a temp file named todo_export_<name_hint>_XXXXXX.txt, blocks while
 * the editor runs, then deletes it regardless of the editor's exit status;
 * the user keeps a copy by saving it elsewhere from the editor.
 *
 * @param name_hint Filename hint (e.g. project name); unsafe characters are
 *                  replaced with '_'. May be NULL.
 */
int notes_editor_view(const char *text, const char *name_hint);

/**
 * @brief Base64-encode @p data into @p out (pure, directly unit-tested).
 * @return RT_ERROR if @p out_cap is smaller than 4*ceil(len/3)+1.
 */
int notes_editor_base64_encode(const unsigned char *data, size_t len, char *out, size_t out_cap);

/**
 * @brief Copy @p text to the system clipboard via an OSC 52 terminal escape sequence.
 *
 * Written straight to stdout: OSC 52 is an out-of-band control sequence the
 * terminal itself consumes, not visible screen content, so unlike
 * notes_editor_edit() this needs no curses suspend/resume. Works over SSH and
 * through tmux/screen passthrough without any xclip/wl-copy/pbcopy
 * dependency, provided the terminal honors OSC 52.
 */
int notes_editor_copy_clipboard(const char *text);

#endif //__TODO_NOTES_EDITOR_H
