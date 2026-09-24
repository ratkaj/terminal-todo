// lspdiag

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <common.h>
#include <logger.h>
#include <report.h>
#include <storage.h>
#include <unity/unity.h>

/* Thursday 2026-09-24 14:05 UTC; setUp() pins TZ to UTC. */
static const time_t NOW = 1790258700;
static const time_t MON_0921 = 1789948800;
static const time_t MON_0914 = 1789344000;
static const time_t SEP_01 = 1788220800;
static const time_t AUG_01 = 1785542400;

static char db_path[] = "/tmp/todo_report_test_XXXXXX";
static int64_t project_id;
static char *text;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_report.log");
	setenv("TZ", "UTC", 1);
	tzset();

	/* A file, not :memory:, so set_completed_at() can reach it through a
	   second connection: storage has no API for back-dating a completion. */
	strcpy(db_path, "/tmp/todo_report_test_XXXXXX");
	int fd = mkstemp(db_path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(db_path));

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "atomrpc");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &project_id));
	text = NULL;
}

void tearDown(void) {
	free(text);
	storage_close();
	unlink(db_path);
	logger_close();
}

static int64_t add(int64_t project, int64_t parent_id, const char *title, priority_t prio) {
	task_t t;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_insert(project, parent_id, title, prio, &t));
	int64_t id = t.id;
	task_model_free(&t);
	return id;
}

static void set_completed_at(int64_t id, time_t at) {
	sqlite3 *db = NULL;
	TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(db_path, &db));
	sqlite3_stmt *stmt = NULL;
	TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_prepare_v2(db,
		"UPDATE task SET status = 1, completed_at = ?2 WHERE id = ?1", -1, &stmt, NULL));
	sqlite3_bind_int64(stmt, 1, id);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)at);
	TEST_ASSERT_EQUAL_INT(SQLITE_DONE, sqlite3_step(stmt));
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

static int64_t builtin_id(const char *name) {
	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(true, &arr, &n);
	int64_t id = -1;
	for (size_t i = 0; i < n; i++)
		if (arr[i].builtin && strcmp(arr[i].display_name, name) == 0)
			id = arr[i].id;
	storage_project_array_free(arr, n);
	return id;
}

/* Position of @p needle in the report, failing the test if it is missing. */
static long pos(const char *needle) {
	const char *p = strstr(text, needle);
	if (p == NULL)
		TEST_FAIL_MESSAGE(needle);
	return (long)(p - text);
}

static void assert_range(report_period_t period, time_t now, time_t start, time_t end) {
	time_t s = 0, e = 0;
	report_period_range(period, now, &s, &e);
	TEST_ASSERT_EQUAL_INT64(start, s);
	TEST_ASSERT_EQUAL_INT64(end, e);
}

void test_report_period_ranges_midweek(void) {
	assert_range(REPORT_THIS_WEEK, NOW, MON_0921, NOW + 1);
	assert_range(REPORT_LAST_WEEK, NOW, MON_0914, MON_0921);
	assert_range(REPORT_THIS_MONTH, NOW, SEP_01, NOW + 1);
	assert_range(REPORT_LAST_MONTH, NOW, AUG_01, SEP_01);
}

void test_report_week_starts_monday_midnight(void) {
	assert_range(REPORT_THIS_WEEK, MON_0921, MON_0921, MON_0921 + 1);
	assert_range(REPORT_LAST_WEEK, MON_0921, MON_0914, MON_0921);
	/* Sunday 23:59 still belongs to the week that began on Monday. */
	assert_range(REPORT_THIS_WEEK, 1790553540, MON_0921, 1790553540 + 1);
}

void test_report_last_month_in_january_is_previous_december(void) {
	/* 2026-01-10 12:00 -> 2025-12-01 .. 2026-01-01 */
	assert_range(REPORT_LAST_MONTH, 1768046400, 1764547200, 1767225600);
}

void test_report_week_across_dst_change_uses_local_midnights(void) {
	/* Central Europe leaves summer time on 2026-10-25, so last week is one
	   hour longer than 7 * 24 h. */
	setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
	tzset();
	time_t wed = 1793185200;        /* 2026-10-28 12:00 CET */
	time_t mon_1026 = 1792969200;   /* 2026-10-26 00:00 CET */
	time_t mon_1019 = 1792360800;   /* 2026-10-19 00:00 CEST */
	assert_range(REPORT_LAST_WEEK, wed, mon_1019, mon_1026);
	TEST_ASSERT_EQUAL_INT64(7 * 86400 + 3600, mon_1026 - mon_1019);
}

void test_report_empty_period(void) {
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_THIS_WEEK, NOW, &text));
	TEST_ASSERT_NOT_NULL(strstr(text, "Completed this week\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "Period:    2026-09-21 to 2026-09-24 (so far)\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "Total:     0 tasks in 0 projects\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "(no tasks completed)\n"));
}

void test_report_groups_by_project_and_orders_by_completion(void) {
	int64_t today = builtin_id("Today");
	int64_t later = add(project_id, 0, "Review event manager PR", PRIORITY_P2);
	int64_t earlier = add(project_id, 0, "Fix broken build", PRIORITY_P1);
	int64_t cert = add(today, 0, "Renew TLS certificate", PRIORITY_P2);
	add(project_id, 0, "Still open", PRIORITY_P3);
	set_completed_at(later, MON_0921 + 2 * 86400);
	set_completed_at(earlier, MON_0921 + 86400);
	set_completed_at(cert, NOW - 60);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_THIS_WEEK, NOW, &text));
	TEST_ASSERT_NOT_NULL(strstr(text, "Total:     3 tasks in 2 projects\n"));
	/* Built-ins first, like the Projects pane. */
	TEST_ASSERT_TRUE(pos("Today (1)\n") < pos("atomrpc (2)\n"));
	TEST_ASSERT_TRUE(pos("2026-09-22  [x] P1  Fix broken build")
		< pos("2026-09-23  [x] P2  Review event manager PR"));
	TEST_ASSERT_NULL(strstr(text, "Still open"));
}

void test_report_excludes_completions_outside_the_range(void) {
	int64_t before = add(project_id, 0, "Last Sunday night", PRIORITY_P3);
	int64_t start = add(project_id, 0, "Monday midnight", PRIORITY_P3);
	set_completed_at(before, MON_0921 - 1);
	set_completed_at(start, MON_0921);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_THIS_WEEK, NOW, &text));
	TEST_ASSERT_NOT_NULL(strstr(text, "Monday midnight"));
	TEST_ASSERT_NULL(strstr(text, "Last Sunday night"));

	free(text);
	text = NULL;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_LAST_WEEK, NOW, &text));
	TEST_ASSERT_NOT_NULL(strstr(text, "Period:    2026-09-14 to 2026-09-20\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "Last Sunday night"));
	TEST_ASSERT_NULL(strstr(text, "Monday midnight"));
}

void test_report_shows_open_parent_for_context(void) {
	int64_t parent = add(project_id, 0, "Fix broken build", PRIORITY_P1);
	int64_t sub = add(project_id, parent, "Bisect the failing commit", PRIORITY_P3);
	set_completed_at(sub, MON_0921 + 3600);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_THIS_WEEK, NOW, &text));
	/* The parent is context, not a completion: no date, open box, not counted. */
	TEST_ASSERT_NOT_NULL(strstr(text, "              [ ] P1  Fix broken build\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "  2026-09-21      [x] P3  Bisect the failing commit\n"));
	TEST_ASSERT_TRUE(pos("Fix broken build") < pos("Bisect the failing commit"));
	TEST_ASSERT_NOT_NULL(strstr(text, "Total:     1 task in 1 project\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "atomrpc (1)\n"));
}

void test_report_includes_archived_tasks_and_projects(void) {
	project_t old = {0};
	snprintf(old.display_name, sizeof(old.display_name), "oldproj");
	int64_t old_id;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&old, &old_id));
	int64_t done = add(old_id, 0, "Ship final release", PRIORITY_P2);
	set_completed_at(done, SEP_01 + 86400);
	int count = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_archive_completed(old_id, &count, true));
	old.id = old_id;
	old.archived = true;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_update(&old));

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, report_completed_text(REPORT_THIS_MONTH, NOW, &text));
	TEST_ASSERT_NOT_NULL(strstr(text, "oldproj (1, archived)\n"));
	TEST_ASSERT_NOT_NULL(strstr(text, "  2026-09-02  [x] P2  Ship final release  (archived)\n"));
}

void test_report_rejects_invalid_period(void) {
	TEST_ASSERT_EQUAL_INT(RT_ERROR, report_completed_text(REPORT_PERIOD_COUNT, NOW, &text));
	TEST_ASSERT_NULL(text);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_report_period_ranges_midweek);
	RUN_TEST(test_report_week_starts_monday_midnight);
	RUN_TEST(test_report_last_month_in_january_is_previous_december);
	RUN_TEST(test_report_week_across_dst_change_uses_local_midnights);
	RUN_TEST(test_report_empty_period);
	RUN_TEST(test_report_groups_by_project_and_orders_by_completion);
	RUN_TEST(test_report_excludes_completions_outside_the_range);
	RUN_TEST(test_report_shows_open_parent_for_context);
	RUN_TEST(test_report_includes_archived_tasks_and_projects);
	RUN_TEST(test_report_rejects_invalid_period);
	return UNITY_END();
}
