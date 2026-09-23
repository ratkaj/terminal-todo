// lspdiag

/**
 * @file export.h
 * @brief Plain-text export of a project's tasks and notes.
 *
 * Pure text rendering over task.c/storage.c - no ncurses, no file output.
 * app_main.c hands the result to notes_editor_view(), so the user saves,
 * prints, or copies it from their own $EDITOR.
 */

#ifndef __TODO_EXPORT_H
#define __TODO_EXPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief Render a project's visible task list (Tasks-pane order) as plain text.
 *
 * @param include_archived Mirrors the Tasks pane's Display archived filter.
 * @param now              Timestamp printed in the "Exported:" header line.
 * @param out_text         Heap-allocated result; caller must free() it.
 * @return RT_SUCCESS or RT_ERROR (unknown project, storage or memory failure).
 */
int export_project_text(int64_t project_id, bool include_archived, time_t now,
                         char **out_text);

#endif //__TODO_EXPORT_H
