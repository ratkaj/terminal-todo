// lspdiag

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fuzzy.h>
#include <utf8.h>

/* Per matched character. The bonuses make "brk rec" rank "broker
   reconnect" above "bark or recall": runs and word starts beat letters
   scattered through a word. The gap cost is capped below the base score, so
   every match stays >= 0. */
#define SCORE_CHAR        16
#define SCORE_CONSECUTIVE 12
#define SCORE_WORD_START   8
#define SCORE_TEXT_START   4
#define GAP_COST_MAX       8

/* Lower-cased copy of @p s on the heap; lower-casing can grow a character
   from 2 to 3 bytes, as in storage.c's sql_fold(). */
static char *fold_dup(const char *s)
{
	size_t cap = strlen(s) * 2 + 1;
	char *out = malloc(cap);
	if (out != NULL)
		utf8_fold(s, out, cap);
	return out;
}

/* Split folded @p s into characters, each packed into a uint32_t from its
   bytes, so two characters are equal exactly when their encodings are. */
static uint32_t *to_chars(const char *s, size_t *out_n)
{
	size_t len = strlen(s);
	uint32_t *chars = malloc((len + 1) * sizeof(*chars));
	if (chars == NULL)
		return NULL;
	size_t n = 0;
	for (size_t i = 0; i < len; ) {
		size_t next = utf8_next(s, i);
		uint32_t c = 0;
		for (size_t k = i; k < next && k < i + 4; k++)
			c = (c << 8) | (unsigned char)s[k];
		chars[n++] = c;
		i = next;
	}
	*out_n = n;
	return chars;
}

/* A character is a word start at the beginning of the text or after ASCII
   that isn't a letter or digit; non-ASCII letters count as word characters. */
static bool is_word_start(const uint32_t *t, size_t i)
{
	if (i == 0)
		return true;
	uint32_t p = t[i - 1];
	if (p >= 0x80)
		return false;
	return !((p >= 'a' && p <= 'z') || (p >= 'A' && p <= 'Z') || (p >= '0' && p <= '9'));
}

/* Best score for @p w (m chars) as a subsequence of @p t (n chars), trying
   each start and matching greedily from there; -1 if it does not match. */
static int score_word(const uint32_t *t, size_t n, const uint32_t *w, size_t m)
{
	int best = -1;
	for (size_t s = 0; s < n; s++) {
		if (t[s] != w[0])
			continue;
		int sc = 0;
		size_t pos = s;
		size_t last = 0;
		size_t k;
		for (k = 0; k < m; k++) {
			while (pos < n && t[pos] != w[k])
				pos++;
			if (pos == n)
				break;
			sc += SCORE_CHAR;
			if (k > 0) {
				size_t gap = pos - last - 1;
				if (gap == 0)
					sc += SCORE_CONSECUTIVE;
				else
					sc -= (gap < GAP_COST_MAX) ? (int)gap : GAP_COST_MAX;
			}
			if (is_word_start(t, pos))
				sc += SCORE_WORD_START;
			if (pos == 0)
				sc += SCORE_TEXT_START;
			last = pos;
			pos++;
		}
		/* A later start has less text left, so it cannot match either. */
		if (k < m)
			break;
		if (sc > best)
			best = sc;
	}
	return best;
}

int fuzzy_score(const char *text, const char *query)
{
	if (text == NULL || query == NULL)
		return -1;

	char *ft = fold_dup(text);
	char *fq = fold_dup(query);
	size_t tn = 0, qn = 0;
	uint32_t *t = (ft != NULL) ? to_chars(ft, &tn) : NULL;
	uint32_t *q = (fq != NULL) ? to_chars(fq, &qn) : NULL;
	int total = -1;
	if (t != NULL && q != NULL) {
		total = 0;
		size_t i = 0;
		while (i < qn && total >= 0) {
			while (i < qn && q[i] == ' ')
				i++;
			size_t start = i;
			while (i < qn && q[i] != ' ')
				i++;
			if (i == start)
				break;
			int sc = score_word(t, tn, q + start, i - start);
			total = (sc < 0) ? -1 : total + sc;
		}
	}
	free(t);
	free(q);
	free(ft);
	free(fq);
	return total;
}

bool fuzzy_words_substring(const char *text, const char *query)
{
	if (text == NULL || query == NULL)
		return false;

	char *ft = fold_dup(text);
	char *fq = fold_dup(query);
	bool all = (ft != NULL && fq != NULL);
	char *save = NULL;
	for (char *w = all ? strtok_r(fq, " ", &save) : NULL; w != NULL;
			w = strtok_r(NULL, " ", &save)) {
		if (strstr(ft, w) == NULL) {
			all = false;
			break;
		}
	}
	free(ft);
	free(fq);
	return all;
}
