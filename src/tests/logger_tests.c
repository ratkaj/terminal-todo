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
	LINFO("file info\n");
	LERR("file error\n");
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
	LINFO("hidden info\n");
	LERR("shown error\n");
	logger_close();

	char buf[1024];
	read_all(path, buf, sizeof(buf));
	unlink(path);
	TEST_ASSERT_NULL(strstr(buf, "hidden info"));
	TEST_ASSERT_NOT_NULL(strstr(buf, "shown error"));
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
	LERR("must not crash\n");
	logger_close();
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_logger_file_backend_writes_levels);
	RUN_TEST(test_logger_level_filters_lower_severity);
	RUN_TEST(test_logger_debug_enabled_reflects_level);
	RUN_TEST(test_logger_unopenable_file_disables_logging);
	return UNITY_END();
}
