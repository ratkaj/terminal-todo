// lspdiag

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <strbuf.h>

void sb_appendf(strbuf_t *sb, const char *fmt, ...)
{
	if (sb->failed)
		return;

	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (n < 0) {
		sb->failed = true;
		return;
	}

	size_t need = sb->len + (size_t)n + 1;
	if (need > sb->cap) {
		size_t cap = sb->cap ? sb->cap : 1024;
		while (cap < need)
			cap *= 2;
		char *tmp = realloc(sb->buf, cap);
		if (tmp == NULL) {
			sb->failed = true;
			return;
		}
		sb->buf = tmp;
		sb->cap = cap;
	}

	va_start(ap, fmt);
	vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
	va_end(ap);
	sb->len += (size_t)n;
}
