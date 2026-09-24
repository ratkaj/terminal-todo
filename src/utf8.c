// lspdiag

#include <stdint.h>
#include <string.h>
#include <wctype.h>

#include <utf8.h>

#define IS_CONT(b) (((unsigned char)(b) & 0xC0) == 0x80)

/* Sequence length for a lead byte, or 0 if it cannot start one. C0/C1
   (overlong ASCII) and F5-FF (beyond U+10FFFF) are never valid. */
static int seq_len(unsigned char lead)
{
	if (lead < 0x80)
		return 1;
	if (lead >= 0xC2 && lead <= 0xDF)
		return 2;
	if (lead >= 0xE0 && lead <= 0xEF)
		return 3;
	if (lead >= 0xF0 && lead <= 0xF4)
		return 4;
	return 0;
}

/* Decode the valid sequence at @p s (at most @p avail bytes).
   @return its length, or 0 if it is invalid, overlong, a surrogate or cut short. */
static int decode(const char *s, size_t avail, uint32_t *cp)
{
	const unsigned char *u = (const unsigned char *)s;
	int n = seq_len(u[0]);
	if (n == 0 || (size_t)n > avail)
		return 0;
	if (n == 1) {
		*cp = u[0];
		return 1;
	}
	uint32_t c = u[0] & (0xFF >> (n + 1));
	for (int i = 1; i < n; i++) {
		if (!IS_CONT(u[i]))
			return 0;
		c = (c << 6) | (u[i] & 0x3F);
	}
	static const uint32_t min_for_len[] = { 0, 0, 0x80, 0x800, 0x10000 };
	if (c < min_for_len[n] || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
		return 0;
	*cp = c;
	return n;
}

static int encode(uint32_t c, char *out)
{
	if (c < 0x80) {
		out[0] = (char)c;
		return 1;
	}
	if (c < 0x800) {
		out[0] = (char)(0xC0 | (c >> 6));
		out[1] = (char)(0x80 | (c & 0x3F));
		return 2;
	}
	if (c < 0x10000) {
		out[0] = (char)(0xE0 | (c >> 12));
		out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
		out[2] = (char)(0x80 | (c & 0x3F));
		return 3;
	}
	out[0] = (char)(0xF0 | (c >> 18));
	out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
	out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
	out[3] = (char)(0x80 | (c & 0x3F));
	return 4;
}

void utf8_acc_reset(utf8_acc_t *acc)
{
	acc->len = 0;
	acc->need = 0;
}

int utf8_acc_feed(utf8_acc_t *acc, unsigned char byte)
{
	if (acc->len == 0) {
		int n = seq_len(byte);
		if (n < 2) {
			utf8_acc_reset(acc);
			return -1;
		}
		acc->buf[0] = (char)byte;
		acc->len = 1;
		acc->need = n;
		return 0;
	}
	if (!IS_CONT(byte)) {
		utf8_acc_reset(acc);
		return -1;
	}
	acc->buf[acc->len++] = (char)byte;
	if (acc->len < acc->need)
		return 0;

	int n = acc->len;
	uint32_t cp;
	utf8_acc_reset(acc);
	return decode(acc->buf, (size_t)n, &cp) == n ? n : -1;
}

size_t utf8_prev(const char *s, size_t pos)
{
	if (pos == 0)
		return 0;
	pos--;
	while (pos > 0 && IS_CONT(s[pos]))
		pos--;
	return pos;
}

size_t utf8_next(const char *s, size_t pos)
{
	if (s[pos] == '\0')
		return pos;
	pos++;
	while (s[pos] != '\0' && IS_CONT(s[pos]))
		pos++;
	return pos;
}

size_t utf8_clip_bytes(const char *s, size_t max_bytes)
{
	size_t len = strlen(s);
	if (len <= max_bytes)
		return len;
	/* s[max_bytes] is the first byte cut off; if it continues a character,
	   back up to where that character starts. */
	size_t cut = max_bytes;
	while (cut > 0 && IS_CONT(s[cut]))
		cut--;
	return cut;
}

void utf8_copy(char *dst, size_t dst_cap, const char *src)
{
	if (dst_cap == 0)
		return;
	size_t n = utf8_clip_bytes(src, dst_cap - 1);
	memcpy(dst, src, n);
	dst[n] = '\0';
}

bool utf8_fold(const char *src, char *dst, size_t dst_cap)
{
	if (dst_cap == 0)
		return false;
	size_t i = 0, j = 0;
	size_t len = strlen(src);
	while (i < len) {
		uint32_t cp;
		char enc[4];
		int in_n = decode(src + i, len - i, &cp);
		int out_n;
		if (in_n == 0) {
			/* Not valid UTF-8: keep the byte so matching still sees it. */
			enc[0] = src[i];
			in_n = out_n = 1;
		} else {
			wint_t lower = towlower((wint_t)cp);
			out_n = encode((uint32_t)lower, enc);
		}
		if (j + (size_t)out_n >= dst_cap) {
			dst[j] = '\0';
			return false;
		}
		memcpy(dst + j, enc, (size_t)out_n);
		j += (size_t)out_n;
		i += (size_t)in_n;
	}
	dst[j] = '\0';
	return true;
}
