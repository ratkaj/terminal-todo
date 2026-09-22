// lspdiag

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <logger.h>
#include <notes_editor.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_notes_editor.log");
}

void tearDown(void) {
	logger_close();
}

void test_notes_editor_tmpfile_roundtrips_text(void) {
	char path[512];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_write_tmpfile("Review on Sep 24\n- item one\n- item two\n", path, sizeof(path)));

	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_read_tmpfile(path, &out));
	TEST_ASSERT_EQUAL_STRING("Review on Sep 24\n- item one\n- item two\n", out);

	free(out);
	unlink(path);
}

void test_notes_editor_tmpfile_null_text_is_empty_file(void) {
	char path[512];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_write_tmpfile(NULL, path, sizeof(path)));

	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_read_tmpfile(path, &out));
	TEST_ASSERT_EQUAL_STRING("", out);

	free(out);
	unlink(path);
}

void test_notes_editor_tmpfile_paths_are_unique(void) {
	char path1[512], path2[512];
	notes_editor_write_tmpfile("a", path1, sizeof(path1));
	notes_editor_write_tmpfile("b", path2, sizeof(path2));

	TEST_ASSERT_FALSE(strcmp(path1, path2) == 0);

	unlink(path1);
	unlink(path2);
}

void test_notes_editor_tmpfile_roundtrips_large_text(void) {
	size_t len = 8192;
	char *big = malloc(len + 1);
	memset(big, 'x', len);
	big[len] = '\0';

	char path[512];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_write_tmpfile(big, path, sizeof(path)));

	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_read_tmpfile(path, &out));
	TEST_ASSERT_EQUAL_STRING(big, out);

	free(big);
	free(out);
	unlink(path);
}

void test_notes_editor_read_tmpfile_rejects_missing_file(void) {
	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, notes_editor_read_tmpfile("/nonexistent/path/x", &out));
}

void test_notes_editor_base64_encode_matches_known_vectors(void) {
	char out[64];

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_base64_encode((const unsigned char *)"", 0, out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("", out);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_base64_encode((const unsigned char *)"f", 1, out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("Zg==", out);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_base64_encode((const unsigned char *)"fo", 2, out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("Zm8=", out);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_base64_encode((const unsigned char *)"foo", 3, out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("Zm9v", out);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		notes_editor_base64_encode((const unsigned char *)"foobar", 6, out, sizeof(out)));
	TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", out);
}

void test_notes_editor_base64_encode_rejects_too_small_buffer(void) {
	char out[4]; /* "foo" needs 4 chars + NUL = 5 */
	TEST_ASSERT_EQUAL_INT(RT_ERROR,
		notes_editor_base64_encode((const unsigned char *)"foo", 3, out, sizeof(out)));
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_notes_editor_tmpfile_roundtrips_text);
	RUN_TEST(test_notes_editor_tmpfile_null_text_is_empty_file);
	RUN_TEST(test_notes_editor_tmpfile_paths_are_unique);
	RUN_TEST(test_notes_editor_tmpfile_roundtrips_large_text);
	RUN_TEST(test_notes_editor_read_tmpfile_rejects_missing_file);
	RUN_TEST(test_notes_editor_base64_encode_matches_known_vectors);
	RUN_TEST(test_notes_editor_base64_encode_rejects_too_small_buffer);
	return UNITY_END();
}
