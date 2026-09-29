// lspdiag

#include <locale.h>

#include <fuzzy.h>
#include <unity/unity.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_fuzzy_matches_an_in_order_subsequence(void) {
	TEST_ASSERT_TRUE(fuzzy_score("Investigate broker reconnect", "brkrec") >= 0);
	TEST_ASSERT_TRUE(fuzzy_score("Investigate broker reconnect", "invest") >= 0);
}

void test_fuzzy_rejects_out_of_order_or_missing_letters(void) {
	TEST_ASSERT_EQUAL_INT(-1, fuzzy_score("broker", "rb"));
	TEST_ASSERT_EQUAL_INT(-1, fuzzy_score("broker", "brokers"));
	TEST_ASSERT_EQUAL_INT(-1, fuzzy_score("", "a"));
}

void test_fuzzy_requires_every_word_in_any_order(void) {
	TEST_ASSERT_TRUE(fuzzy_score("Test MQTT recovery", "recov mqtt") >= 0);
	TEST_ASSERT_EQUAL_INT(-1, fuzzy_score("Test MQTT recovery", "mqtt acl"));
}

void test_fuzzy_empty_query_matches_with_zero(void) {
	TEST_ASSERT_EQUAL_INT(0, fuzzy_score("anything", ""));
	TEST_ASSERT_EQUAL_INT(0, fuzzy_score("anything", "   "));
}

void test_fuzzy_ranks_runs_above_scattered_letters(void) {
	int run = fuzzy_score("reconnect broker", "rec");
	int scattered = fuzzy_score("review the check", "rec");
	TEST_ASSERT_TRUE(run > scattered);
}

void test_fuzzy_ranks_word_starts_above_mid_word(void) {
	int start = fuzzy_score("fix the cat", "cat");
	int mid = fuzzy_score("fix the locate", "cat");
	TEST_ASSERT_TRUE(start > mid);
}

void test_fuzzy_prefers_the_best_start(void) {
	/* The first 'b' leads to a scattered match; the later "broker" is a run. */
	int later = fuzzy_score("bad xx broker", "broker");
	int alone = fuzzy_score("broker", "broker");
	TEST_ASSERT_TRUE(later >= alone - 4);  /* only the text-start bonus differs */
}

void test_fuzzy_folds_case_in_any_script(void) {
	TEST_ASSERT_TRUE(fuzzy_score("Čvor MQTT", "čvor mqtt") >= 0);
	TEST_ASSERT_TRUE(fuzzy_score("čvor", "ČVOR") >= 0);
	/* Accents are not folded away, matching the project switcher. */
	TEST_ASSERT_EQUAL_INT(-1, fuzzy_score("Čvor", "cvor"));
}

void test_fuzzy_words_substring_needs_contiguous_words(void) {
	const char *notes = "Broker drops after 30s idle;\ncheck KEEPALIVE.";
	TEST_ASSERT_TRUE(fuzzy_words_substring(notes, "keepalive broker"));
	TEST_ASSERT_TRUE(fuzzy_words_substring(notes, "drops"));
	TEST_ASSERT_FALSE(fuzzy_words_substring(notes, "brkr"));
	TEST_ASSERT_FALSE(fuzzy_words_substring(notes, "broker timeout"));
}

void test_fuzzy_words_substring_null_and_empty(void) {
	TEST_ASSERT_FALSE(fuzzy_words_substring(NULL, "x"));
	TEST_ASSERT_TRUE(fuzzy_words_substring("notes", ""));
}

int main(void) {
	/* towlower() maps non-ASCII letters only in a UTF-8 LC_CTYPE. */
	TEST_ASSERT_NOT_NULL(setlocale(LC_CTYPE, "C.UTF-8"));

	UNITY_BEGIN();
	RUN_TEST(test_fuzzy_matches_an_in_order_subsequence);
	RUN_TEST(test_fuzzy_rejects_out_of_order_or_missing_letters);
	RUN_TEST(test_fuzzy_requires_every_word_in_any_order);
	RUN_TEST(test_fuzzy_empty_query_matches_with_zero);
	RUN_TEST(test_fuzzy_ranks_runs_above_scattered_letters);
	RUN_TEST(test_fuzzy_ranks_word_starts_above_mid_word);
	RUN_TEST(test_fuzzy_prefers_the_best_start);
	RUN_TEST(test_fuzzy_folds_case_in_any_script);
	RUN_TEST(test_fuzzy_words_substring_needs_contiguous_words);
	RUN_TEST(test_fuzzy_words_substring_null_and_empty);
	return UNITY_END();
}
