// lspdiag

#include <logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unity/unity.h>

void setUp(void) {
}

void tearDown(void) {
}

static void read_all(const char *path, char *buf, size_t size) {
	FILE *in = fopen(path, "r");
	TEST_ASSERT_NOT_NULL(in);
	size_t n = fread(buf, 1, size - 1, in);
	buf[n] = '\0';
	fclose(in);
}

void test_logger_file_backend_writes_levels(void) {
	char path[] = "/tmp/todo_logger_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, path);
	LINFO("file info");
	LERR("file error");
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	TEST_ASSERT_NOT_NULL(strstr(buf, "[INFO]: file info"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "[ERROR]: file error"));
}

void test_logger_level_filters_lower_severity(void) {
	char path[] = "/tmp/todo_logger_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	logger_init(LOG_LVL_ERROR, LOG_BACKEND_FILE, path);
	LINFO("hidden info");
	LERR("shown error");
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	TEST_ASSERT_NULL(strstr(buf, "hidden info"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "shown error"));
}

void test_logger_file_backend_ends_each_message_with_newline(void) {
	char path[] = "/tmp/todo_logger_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, path);
	LINFO("first");
	LINFO("second");
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	TEST_ASSERT_NOT_NULL(strstr(buf, "[INFO]: first\n"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "[INFO]: second\n"));
}

void test_logger_copies_file_path(void) {
	char path[] = "/tmp/todo_logger_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	char tmp[sizeof(path)];
	memcpy(tmp, path, sizeof(path));
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_STDIO, tmp);
	memset(tmp, 'x', sizeof(tmp) - 1);
	/* The stdio backend prefixes each line with the id; it must still be
	 * the original string after the caller's buffer changes. */
	FILE *saved = stdout;
	stdout = fopen(path, "w");
	TEST_ASSERT_NOT_NULL(stdout);
	LINFO("copied");
	fclose(stdout);
	stdout = saved;
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	char want[64];
	snprintf(want, sizeof(want), "%s [INFO]: copied\n", path);
	TEST_ASSERT_EQUAL_STRING(want, buf);
}

void test_logger_out_of_range_level_is_clamped(void) {
	char path[] = "/tmp/todo_logger_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	/* An out-of-range configured level is clamped to DEBUG, so it cannot
	 * let an out-of-range message level through to level_str. */
	logger_init((log_level_t)99, LOG_BACKEND_FILE, path);
	TEST_ASSERT_TRUE(logger_debug_enabled());
	logger_log((log_level_t)99, "odd level");
	LDBG("debug level");
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	TEST_ASSERT_NULL(strstr(buf, "odd level"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "[DEBUG]: debug level\n"));
}

void test_logger_debug_enabled_reflects_level(void) {
	TEST_ASSERT_FALSE(logger_debug_enabled());

	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_STDIO, "todo-testing");
	TEST_ASSERT_TRUE(logger_debug_enabled());
	logger_close();

	logger_init(LOG_LVL_INFO, LOG_BACKEND_STDIO, "todo-testing");
	TEST_ASSERT_FALSE(logger_debug_enabled());
	logger_close();

	TEST_ASSERT_FALSE(logger_debug_enabled());
}

void test_logger_unopenable_file_disables_logging(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/nonexistent-dir/todo.log");
	TEST_ASSERT_FALSE(logger_debug_enabled());
	LERR("must not crash");
	logger_close();
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_logger_file_backend_writes_levels);
	RUN_TEST(test_logger_level_filters_lower_severity);
	RUN_TEST(test_logger_file_backend_ends_each_message_with_newline);
	RUN_TEST(test_logger_copies_file_path);
	RUN_TEST(test_logger_out_of_range_level_is_clamped);
	RUN_TEST(test_logger_debug_enabled_reflects_level);
	RUN_TEST(test_logger_unopenable_file_disables_logging);
	return UNITY_END();
}
