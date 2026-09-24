// lspdiag

/**
 * @file utf8.h
 * @brief Byte-level UTF-8 helpers for text entry, safe truncation and
 *        case-insensitive matching.
 *
 * Pure functions with no ncurses or locale-dependent decoding, except
 * utf8_fold(), whose case mapping uses towlower() and so follows LC_CTYPE.
 * Display-width measurement stays in ui_draw.c (wcwidth()).
 */

#ifndef __TODO_UTF8_H
#define __TODO_UTF8_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @struct utf8_acc_t
 * @brief Collects the bytes of one multi-byte character typed as separate
 *        keys (wgetch() returns a UTF-8 character one byte at a time).
 *        Zero-initialize before use.
 */
typedef struct {
	char buf[4];
	int len;
	int need;
} utf8_acc_t;

/**
 * @brief Feed one byte (0x80-0xFF) into @p acc.
 * @return the length of the completed character now in acc->buf, 0 while
 *         more bytes are needed, or -1 if the byte cannot continue or start
 *         a valid sequence (the partial character is discarded).
 */
int utf8_acc_feed(utf8_acc_t *acc, unsigned char byte);

/** @brief Discard any partially collected character. */
void utf8_acc_reset(utf8_acc_t *acc);

/** @brief Byte offset of the character boundary before @p pos in @p s (0 at the start). */
size_t utf8_prev(const char *s, size_t pos);

/** @brief Byte offset of the character boundary after @p pos in @p s (strlen at the end). */
size_t utf8_next(const char *s, size_t pos);

/**
 * @brief Largest length <= @p max_bytes (and <= strlen(s)) that does not
 *        end inside a multi-byte character.
 */
size_t utf8_clip_bytes(const char *s, size_t max_bytes);

/** @brief Copy @p src into @p dst (cap @p dst_cap), cutting only at a character boundary. */
void utf8_copy(char *dst, size_t dst_cap, const char *src);

/**
 * @brief Write a lower-cased copy of @p src into @p dst for case-insensitive
 *        matching (towlower() per character). Invalid bytes are copied as is.
 * @return false if @p dst_cap was too small (the output is then cut short).
 */
bool utf8_fold(const char *src, char *dst, size_t dst_cap);

#endif //__TODO_UTF8_H
