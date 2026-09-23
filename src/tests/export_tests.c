// lspdiag

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <common.h>
#include <export.h>
#include <logger.h>
#include <storage.h>
#include <task.h>
#include <unity/unity.h>

static int64_t project_id;
static char *text;

/* Fixed export timestamp, rendered in local time like the header itself. */
static const time_t NOW = 1790000000;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_export.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "atomrpc");
	p.canonical_path = "/home/user/work/atomrpc";
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &project_id));
	text = NULL;
}

void tearDown(void) {
	free(text);
	storage_close();
	logger_close();
}

static int64_t add(int64_t parent_id, const char *title, priority_t prio, const char *notes) {
	task_t t;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_create(project_id, parent_id, title, prio, &t));
	if (notes != NULL)
		TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_update_fields(t.id, NULL, notes));
	int64_t id = t.id;
	task_model_free(&t);
	return id;
}

static void header(char *buf, size_t cap, const char *tasks_line) {
	char stamp[32];
	struct tm tm;
	localtime_r(&NOW, &tm);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm);
	snprintf(buf, cap,
		"atomrpc\n"
		"Path:     /home/user/work/atomrpc\n"
		"Exported: %s\n"
		"Tasks:    %s\n\n", stamp, tasks_line);
}

void test_export_empty_project(void) {
	char expected[512];
	header(expected, sizeof(expected), "0 open, 0 completed");
	strcat(expected, "(no tasks)\n");

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, export_project_text(project_id, false, NOW, &text));
	TEST_ASSERT_EQUAL_STRING(expected, text);
}

void test_export_full_layout_matches_tasks_pane_order(void) {
	int64_t disc = add(0, "Implement project discovery", PRIORITY_P1,
		"Walk up from current directory.\n\nReview on Sep 24\n\n\n");
	add(disc, "Query registered paths", PRIORITY_P3, NULL);
	int64_t walk = add(disc, "Walk parent directories", PRIORITY_P3, NULL);
	add(disc, "Add tests", PRIORITY_P2, "sub note\r\n");
	add(0, "Write initial test suite", PRIORITY_P3, NULL);
	add(0, "Set up CI build", PRIORITY_P3, "   \n");
	add(0, "Improve ncurses UI", PRIORITY_P2, NULL);
	int count;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(walk, true, false, &count));

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, export_project_text(project_id, false, NOW, &text));

	char expected[2048];
	header(expected, sizeof(expected), "4 open, 0 completed");
	strcat(expected,
		"[ ] P1  Implement project discovery\n"
		"        | Walk up from current directory.\n"
		"        |\n"
		"        | Review on Sep 24\n"
		"    [ ] P2  Add tests\n"
		"            | sub note\n"
		"    [ ] P3  Query registered paths\n"
		"    [x] P3  Walk parent directories\n"
		"\n"
		"[ ] P2  Improve ncurses UI\n"
		"[ ] P3  Write initial test suite\n"
		"[ ] P3  Set up CI build\n");
	TEST_ASSERT_EQUAL_STRING(expected, text);
}

void test_export_completed_date_and_archived_filter(void) {
	add(0, "Open task", PRIORITY_P3, NULL);
	int64_t done = add(0, "Done task", PRIORITY_P3, NULL);
	int64_t old = add(0, "Old task", PRIORITY_P3, NULL);
	int count;
	task_set_completed(old, true, false, &count);
	task_archive_completed(project_id, &count, true);
	task_set_completed(done, true, false, &count);

	char day[16];
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(day, sizeof(day), "%Y-%m-%d", &tm);

	char expected[1024], body[512];
	header(expected, sizeof(expected), "1 open, 1 completed");
	snprintf(body, sizeof(body),
		"[ ] P3  Open task\n"
		"\n"
		"[x] P3  Done task\n"
		"        (completed %s)\n", day);
	strcat(expected, body);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, export_project_text(project_id, false, NOW, &text));
	TEST_ASSERT_EQUAL_STRING(expected, text);
	free(text);
	text = NULL;

	header(expected, sizeof(expected), "1 open, 1 completed, 1 archived");
	snprintf(body, sizeof(body),
		"[ ] P3  Open task\n"
		"\n"
		"[x] P3  Done task\n"
		"        (completed %s)\n"
		"\n"
		"[x] P3  Old task  (archived)\n"
		"        (completed %s)\n", day, day);
	strcat(expected, body);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, export_project_text(project_id, true, NOW, &text));
	TEST_ASSERT_EQUAL_STRING(expected, text);
}

void test_export_named_project_has_no_path_line(void) {
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "Errands");
	int64_t id;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, export_project_text(id, false, NOW, &text));
	TEST_ASSERT_EQUAL_STRING_LEN("Errands\nExported: ", text, 18);
	TEST_ASSERT_NULL(strstr(text, "Path:"));
}

void test_export_unknown_project_fails(void) {
	TEST_ASSERT_EQUAL_INT(RT_ERROR, export_project_text(99999, false, NOW, &text));
	TEST_ASSERT_NULL(text);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_export_empty_project);
	RUN_TEST(test_export_full_layout_matches_tasks_pane_order);
	RUN_TEST(test_export_completed_date_and_archived_filter);
	RUN_TEST(test_export_named_project_has_no_path_line);
	RUN_TEST(test_export_unknown_project_fails);
	return UNITY_END();
}
