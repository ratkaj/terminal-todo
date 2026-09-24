// lspdiag

/**
 * @file strbuf.h
 * @brief Growable heap string for building plain-text output (export, reports).
 *
 * Errors are sticky: after a failed allocation every further append is a
 * no-op and `failed` stays true, so callers check once at the end.
 */

#ifndef __TODO_STRBUF_H
#define __TODO_STRBUF_H

#include <stdbool.h>
#include <stddef.h>

/** @struct strbuf_t Zero-initialize before use; the caller owns and free()s buf. */
typedef struct {
	char *buf;
	size_t len;
	size_t cap;
	bool failed;
} strbuf_t;

/** @brief Append printf-formatted text, growing the buffer as needed. */
void sb_appendf(strbuf_t *sb, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif //__TODO_STRBUF_H
