// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

void test_notes_editor_edit_passes_tmpdir_with_spaces_and_quotes_as_one_path(void) {
	char root[] = "/tmp/todo_notes_editor_test_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(root));

	/* A $TMPDIR the shell would split or expand if the path were unquoted. */
	char tmpdir[256];
	snprintf(tmpdir, sizeof(tmpdir), "%s/my dir's $HOME;x", root);
	TEST_ASSERT_EQUAL_INT(0, mkdir(tmpdir, 0700));

	/* Stub editor: replace the file's contents, fail on anything but one arg. */
	char stub[256];
	snprintf(stub, sizeof(stub), "%s/stub-editor", root);
	FILE *sf = fopen(stub, "w");
	TEST_ASSERT_NOT_NULL(sf);
	fputs("#!/bin/sh\n[ $# -eq 1 ] || exit 3\nprintf edited > \"$1\"\n", sf);
	fclose(sf);
	TEST_ASSERT_EQUAL_INT(0, chmod(stub, 0700));

	const char *old_tmpdir = getenv("TMPDIR");
	char *saved_tmpdir = old_tmpdir ? strdup(old_tmpdir) : NULL;
	const char *old_editor = getenv("EDITOR");
	char *saved_editor = old_editor ? strdup(old_editor) : NULL;
	setenv("TMPDIR", tmpdir, 1);
	setenv("EDITOR", stub, 1);

	char *out = NULL;
	int rc = notes_editor_edit("hello", &out);

	if (saved_tmpdir) setenv("TMPDIR", saved_tmpdir, 1); else unsetenv("TMPDIR");
	if (saved_editor) setenv("EDITOR", saved_editor, 1); else unsetenv("EDITOR");
	free(saved_tmpdir);
	free(saved_editor);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, rc);
	TEST_ASSERT_EQUAL_STRING("edited", out);
	free(out);

	unlink(stub);
	rmdir(tmpdir);
	rmdir(root);
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
	RUN_TEST(test_notes_editor_edit_passes_tmpdir_with_spaces_and_quotes_as_one_path);
	RUN_TEST(test_notes_editor_base64_encode_matches_known_vectors);
	RUN_TEST(test_notes_editor_base64_encode_rejects_too_small_buffer);
	return UNITY_END();
}
