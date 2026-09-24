// lspdiag

#include <locale.h>
#include <string.h>

#include <utf8.h>
#include <unity/unity.h>

void setUp(void) {
}

void tearDown(void) {
}

/* Feed every byte of @p s; @return the last feed result. */
static int feed_all(utf8_acc_t *acc, const char *s) {
	int rc = -2;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++)
		rc = utf8_acc_feed(acc, *p);
	return rc;
}

void test_utf8_acc_completes_croatian_letter(void) {
	utf8_acc_t acc = {0};
	TEST_ASSERT_EQUAL_INT(0, utf8_acc_feed(&acc, 0xC4)); /* č = C4 8D */
	TEST_ASSERT_EQUAL_INT(2, utf8_acc_feed(&acc, 0x8D));
	TEST_ASSERT_EQUAL_MEMORY("\xC4\x8D", acc.buf, 2);
}

void test_utf8_acc_completes_three_and_four_byte_characters(void) {
	utf8_acc_t acc = {0};
	TEST_ASSERT_EQUAL_INT(3, feed_all(&acc, "日"));
	TEST_ASSERT_EQUAL_MEMORY("日", acc.buf, 3);
	TEST_ASSERT_EQUAL_INT(4, feed_all(&acc, "😀"));
	TEST_ASSERT_EQUAL_MEMORY("😀", acc.buf, 4);
}

void test_utf8_acc_rejects_invalid_bytes(void) {
	utf8_acc_t acc = {0};
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0x8D));   /* stray continuation */
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0xC0));   /* overlong lead */
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0xFF));
	TEST_ASSERT_EQUAL_INT(0, utf8_acc_feed(&acc, 0xE0));
	TEST_ASSERT_EQUAL_INT(0, utf8_acc_feed(&acc, 0x80));
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0x80));   /* overlong 3-byte */
	TEST_ASSERT_EQUAL_INT(-1, feed_all(&acc, "\xED\xA0\x80")); /* surrogate */
	/* A new lead byte where a continuation belongs drops the partial one. */
	TEST_ASSERT_EQUAL_INT(0, utf8_acc_feed(&acc, 0xC4));
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0xC5));
	TEST_ASSERT_EQUAL_INT(2, feed_all(&acc, "š"));
}

void test_utf8_acc_reset_discards_partial_character(void) {
	utf8_acc_t acc = {0};
	TEST_ASSERT_EQUAL_INT(0, utf8_acc_feed(&acc, 0xC4));
	utf8_acc_reset(&acc);
	TEST_ASSERT_EQUAL_INT(-1, utf8_acc_feed(&acc, 0x8D));
}

void test_utf8_prev_and_next_step_whole_characters(void) {
	const char *s = "ač日😀";  /* 1 + 2 + 3 + 4 bytes */
	TEST_ASSERT_EQUAL_size_t(1, utf8_next(s, 0));
	TEST_ASSERT_EQUAL_size_t(3, utf8_next(s, 1));
	TEST_ASSERT_EQUAL_size_t(6, utf8_next(s, 3));
	TEST_ASSERT_EQUAL_size_t(10, utf8_next(s, 6));
	TEST_ASSERT_EQUAL_size_t(10, utf8_next(s, 10));

	TEST_ASSERT_EQUAL_size_t(6, utf8_prev(s, 10));
	TEST_ASSERT_EQUAL_size_t(3, utf8_prev(s, 6));
	TEST_ASSERT_EQUAL_size_t(1, utf8_prev(s, 3));
	TEST_ASSERT_EQUAL_size_t(0, utf8_prev(s, 1));
	TEST_ASSERT_EQUAL_size_t(0, utf8_prev(s, 0));
}

void test_utf8_clip_bytes_never_splits_a_character(void) {
	const char *s = "ač日";  /* boundaries at 0, 1, 3, 6 */
	TEST_ASSERT_EQUAL_size_t(1, utf8_clip_bytes(s, 1));
	TEST_ASSERT_EQUAL_size_t(1, utf8_clip_bytes(s, 2));
	TEST_ASSERT_EQUAL_size_t(3, utf8_clip_bytes(s, 3));
	TEST_ASSERT_EQUAL_size_t(3, utf8_clip_bytes(s, 5));
	TEST_ASSERT_EQUAL_size_t(6, utf8_clip_bytes(s, 6));
	TEST_ASSERT_EQUAL_size_t(6, utf8_clip_bytes(s, 99));
}

void test_utf8_copy_cuts_at_a_character_boundary(void) {
	char dst[4];
	utf8_copy(dst, sizeof(dst), "ačž");  /* 5 bytes; 3 fit, but ž would split */
	TEST_ASSERT_EQUAL_STRING("ač", dst);
}

void test_utf8_fold_lowercases_non_ascii(void) {
	char out[64];
	TEST_ASSERT_TRUE(utf8_fold("ČVOR Šuma ĐAK", out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("čvor šuma đak", out);
}

void test_utf8_fold_keeps_invalid_bytes_and_reports_short_buffer(void) {
	char out[64];
	TEST_ASSERT_TRUE(utf8_fold("A\xFF" "B", out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("a\xFF" "b", out);

	char small[3];
	TEST_ASSERT_FALSE(utf8_fold("ČČ", small, sizeof(small)));
	TEST_ASSERT_EQUAL_STRING("č", small);
}

int main(void) {
	/* towlower() maps non-ASCII letters only in a UTF-8 LC_CTYPE, as the
	   app gets from setlocale(LC_ALL, "") under a UTF-8 locale. */
	TEST_ASSERT_NOT_NULL(setlocale(LC_CTYPE, "C.UTF-8"));

	UNITY_BEGIN();
	RUN_TEST(test_utf8_acc_completes_croatian_letter);
	RUN_TEST(test_utf8_acc_completes_three_and_four_byte_characters);
	RUN_TEST(test_utf8_acc_rejects_invalid_bytes);
	RUN_TEST(test_utf8_acc_reset_discards_partial_character);
	RUN_TEST(test_utf8_prev_and_next_step_whole_characters);
	RUN_TEST(test_utf8_clip_bytes_never_splits_a_character);
	RUN_TEST(test_utf8_copy_cuts_at_a_character_boundary);
	RUN_TEST(test_utf8_fold_lowercases_non_ascii);
	RUN_TEST(test_utf8_fold_keeps_invalid_bytes_and_reports_short_buffer);
	return UNITY_END();
}
