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

#endif //__TODO_NOTES_EDITOR_H
