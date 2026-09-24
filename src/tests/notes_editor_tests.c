// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common.h>
#include <logger.h>
#include <notes_editor.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_notes_editor.log");
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
	char msg[256];
	int rc = notes_editor_edit("hello", &out, msg, sizeof(msg));

	if (saved_tmpdir) setenv("TMPDIR", saved_tmpdir, 1); else unsetenv("TMPDIR");
	if (saved_editor) setenv("EDITOR", saved_editor, 1); else unsetenv("EDITOR");
	free(saved_tmpdir);
	free(saved_editor);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, rc);
	TEST_ASSERT_EQUAL_STRING("edited", out);
	TEST_ASSERT_EQUAL_STRING("", msg);
	free(out);

	unlink(stub);
	rmdir(tmpdir);
	rmdir(root);
}

void test_notes_editor_keep_unsaved_writes_file_and_message_with_path(void) {
	char msg[4352];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_keep_unsaved("lost edit\n", msg, sizeof(msg)));

	const char *nl = strchr(msg, '\n');
	TEST_ASSERT_NOT_NULL(nl);
	TEST_ASSERT_EQUAL_INT(0, strncmp(msg, "Notes not saved", 15));
	TEST_ASSERT_EQUAL_INT(0, strncmp(nl + 1, "Kept in ", 8));

	const char *path = nl + 1 + 8;
	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_read_tmpfile(path, &out));
	TEST_ASSERT_EQUAL_STRING("lost edit\n", out);
	free(out);
	unlink(path);
}

void test_notes_editor_keep_unsaved_falls_back_to_tmp_when_tmpdir_is_unusable(void) {
	const char *old = getenv("TMPDIR");
	char *saved = old ? strdup(old) : NULL;
	setenv("TMPDIR", "/nonexistent/todo-test-dir", 1);

	char msg[4352];
	int rc = notes_editor_keep_unsaved("lost edit", msg, sizeof(msg));

	if (saved) setenv("TMPDIR", saved, 1); else unsetenv("TMPDIR");
	free(saved);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, rc);
	const char *nl = strchr(msg, '\n');
	TEST_ASSERT_NOT_NULL(nl);
	TEST_ASSERT_EQUAL_INT(0, strncmp(nl + 1, "Kept in /tmp/todo_unsaved_notes_", 32));
	unlink(nl + 1 + 8);
}

void test_notes_editor_keep_unsaved_reports_when_file_cannot_be_written(void) {
	/* With no free file descriptors, neither $TMPDIR nor /tmp can be used. */
	struct rlimit saved_lim;
	TEST_ASSERT_EQUAL_INT(0, getrlimit(RLIMIT_NOFILE, &saved_lim));
	int probe = dup(0);
	TEST_ASSERT_TRUE(probe >= 0);
	close(probe);
	struct rlimit lim = saved_lim;
	lim.rlim_cur = (rlim_t)probe;
	TEST_ASSERT_EQUAL_INT(0, setrlimit(RLIMIT_NOFILE, &lim));

	char msg[256];
	int rc = notes_editor_keep_unsaved("lost edit", msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(0, setrlimit(RLIMIT_NOFILE, &saved_lim));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_EQUAL_INT(0, strncmp(msg, "Notes not saved", 15));
	TEST_ASSERT_NULL(strchr(msg, '\n'));
}

void test_notes_editor_read_tmpfile_drops_nul_bytes(void) {
	char path[] = "/tmp/todo_notes_nul_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	TEST_ASSERT_EQUAL_INT(7, (int)write(fd, "ab\0cd\0e", 7));
	close(fd);

	char *out = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, notes_editor_read_tmpfile(path, &out));
	TEST_ASSERT_EQUAL_STRING("abcde", out);
	free(out);
	unlink(path);
}

/* A $TMPDIR and $EDITOR set up for one editor-stub test, restored by
   stub_env_end(). The stub is a shell script with @p body. */
typedef struct {
	char root[64];
	char tmpdir[128];
	char stub[128];
	char *saved_tmpdir;
	char *saved_editor;
} stub_env_t;

static void stub_env_begin(stub_env_t *e, const char *editor_value, const char *body) {
	snprintf(e->root, sizeof(e->root), "/tmp/todo_notes_editor_test_XXXXXX");
	TEST_ASSERT_NOT_NULL(mkdtemp(e->root));
	snprintf(e->tmpdir, sizeof(e->tmpdir), "%s/tmp", e->root);
	TEST_ASSERT_EQUAL_INT(0, mkdir(e->tmpdir, 0700));
	snprintf(e->stub, sizeof(e->stub), "%s/stub-editor", e->root);
	if (body != NULL) {
		FILE *sf = fopen(e->stub, "w");
		TEST_ASSERT_NOT_NULL(sf);
		fprintf(sf, "#!/bin/sh\n%s\n", body);
		fclose(sf);
		TEST_ASSERT_EQUAL_INT(0, chmod(e->stub, 0700));
	}

	const char *t = getenv("TMPDIR");
	const char *ed = getenv("EDITOR");
	e->saved_tmpdir = t ? strdup(t) : NULL;
	e->saved_editor = ed ? strdup(ed) : NULL;
	setenv("TMPDIR", e->tmpdir, 1);
	setenv("EDITOR", editor_value != NULL ? editor_value : e->stub, 1);
}

/* @return the number of files left in the stub's $TMPDIR, which is removed. */
static int stub_env_end(stub_env_t *e) {
	if (e->saved_tmpdir) setenv("TMPDIR", e->saved_tmpdir, 1); else unsetenv("TMPDIR");
	if (e->saved_editor) setenv("EDITOR", e->saved_editor, 1); else unsetenv("EDITOR");
	free(e->saved_tmpdir);
	free(e->saved_editor);

	char cmd[256];
	snprintf(cmd, sizeof(cmd), "ls -A '%s' | wc -l", e->tmpdir);
	FILE *p = popen(cmd, "r");
	int left = -1;
	if (p != NULL) {
		if (fscanf(p, "%d", &left) != 1)
			left = -1;
		pclose(p);
	}
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", e->root);
	TEST_ASSERT_EQUAL_INT(0, system(cmd));
	return left;
}

void test_notes_editor_edit_cancel_is_silent(void) {
	stub_env_t e;
	stub_env_begin(&e, NULL, "exit 1");

	char *out = NULL;
	char msg[256] = "x";
	int rc = notes_editor_edit("hello", &out, msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(0, stub_env_end(&e));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_NULL(out);
	TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_notes_editor_edit_reports_missing_editor(void) {
	stub_env_t e;
	stub_env_begin(&e, "todo-no-such-editor", NULL);

	char *out = NULL;
	char msg[256];
	int rc = notes_editor_edit("hello", &out, msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(0, stub_env_end(&e));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_NULL(out);
	TEST_ASSERT_EQUAL_STRING(
		"Could not run \"todo-no-such-editor\"; set $EDITOR to an installed editor.\n"
		"Notes unchanged.", msg);
}

void test_notes_editor_edit_reports_editor_killed_by_signal(void) {
	stub_env_t e;
	stub_env_begin(&e, NULL, "kill -KILL $$");

	char *out = NULL;
	char msg[256];
	int rc = notes_editor_edit("hello", &out, msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(0, stub_env_end(&e));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_NOT_NULL(strstr(msg, "killed by signal 9"));
}

void test_notes_editor_edit_reports_editor_killed_under_a_forking_shell(void) {
	/* dash (Debian/Ubuntu /bin/sh) forks the editor instead of exec'ing it,
	   so an editor killed by SIGKILL shows up as the shell exiting 137
	   (128 + 9); bash execs it and the signal arrives directly. The stub
	   reproduces dash's view: a child killed by the signal, then exit 128+N. */
	stub_env_t e;
	stub_env_begin(&e, NULL, "sh -c 'kill -KILL $$'\nexit $?");

	char *out = NULL;
	char msg[256];
	int rc = notes_editor_edit("hello", &out, msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(0, stub_env_end(&e));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_NOT_NULL(strstr(msg, "killed by signal 9"));
}

void test_notes_editor_view_reports_missing_editor_and_keeps_file(void) {
	stub_env_t e;
	stub_env_begin(&e, "todo-no-such-editor", NULL);

	char msg[4352];
	int rc = notes_editor_view("report text", "report_this_week", msg, sizeof(msg));

	char want[256];
	snprintf(want, sizeof(want), "\nThe text is in %s/todo_report_this_week_", e.tmpdir);
	int left = stub_env_end(&e);
	TEST_ASSERT_EQUAL_INT(RT_ERROR, rc);
	TEST_ASSERT_EQUAL_INT(1, left);
	TEST_ASSERT_EQUAL_INT(0, strncmp(msg, "Could not run \"todo-no-such-editor\"", 35));
	TEST_ASSERT_NOT_NULL(strstr(msg, want));
}

void test_notes_editor_view_nonzero_exit_is_not_an_error(void) {
	stub_env_t e;
	stub_env_begin(&e, NULL, "exit 1");

	char msg[256] = "x";
	int rc = notes_editor_view("report text", "report", msg, sizeof(msg));

	stub_env_end(&e);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, rc);
	TEST_ASSERT_EQUAL_STRING("", msg);
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
	RUN_TEST(test_notes_editor_keep_unsaved_writes_file_and_message_with_path);
	RUN_TEST(test_notes_editor_keep_unsaved_falls_back_to_tmp_when_tmpdir_is_unusable);
	RUN_TEST(test_notes_editor_keep_unsaved_reports_when_file_cannot_be_written);
	RUN_TEST(test_notes_editor_read_tmpfile_drops_nul_bytes);
	RUN_TEST(test_notes_editor_edit_cancel_is_silent);
	RUN_TEST(test_notes_editor_edit_reports_missing_editor);
	RUN_TEST(test_notes_editor_edit_reports_editor_killed_by_signal);
	RUN_TEST(test_notes_editor_edit_reports_editor_killed_under_a_forking_shell);
	RUN_TEST(test_notes_editor_view_reports_missing_editor_and_keeps_file);
	RUN_TEST(test_notes_editor_view_nonzero_exit_is_not_an_error);
	RUN_TEST(test_notes_editor_base64_encode_matches_known_vectors);
	RUN_TEST(test_notes_editor_base64_encode_rejects_too_small_buffer);
	return UNITY_END();
}
