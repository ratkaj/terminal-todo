// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <logger.h>
#include <storage.h>
#include <unity/unity.h>

static int64_t project_id;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_storage.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "atomrpc");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &project_id));
}

void tearDown(void) {
	storage_close();
	logger_close();
}

static int64_t find_builtin_id(const char *name) {
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

void test_storage_open_seeds_builtin_projects(void) {
	TEST_ASSERT_NOT_EQUAL(-1, find_builtin_id("Today"));
	TEST_ASSERT_NOT_EQUAL(-1, find_builtin_id("This Week"));
	TEST_ASSERT_NOT_EQUAL(-1, find_builtin_id("Inbox"));
}

void test_storage_open_seeding_is_idempotent_across_reopens(void) {
	char path[] = "/tmp/todo_storage_test_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	/* Close the :memory: DB from setUp and reopen against the temp file twice. */
	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(path));
	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(path));

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(true, &arr, &n);
	int builtin_count = 0;
	for (size_t i = 0; i < n; i++)
		if (arr[i].builtin)
			builtin_count++;
	storage_project_array_free(arr, n);
	TEST_ASSERT_EQUAL_INT(3, builtin_count);

	unlink(path);
}

void test_storage_project_find_by_path_not_found(void) {
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_project_find_by_path("/nowhere", &out));
}

void test_storage_task_list_top_level_orders_by_state_priority_manual_order(void) {
	task_t out;
	storage_task_insert(project_id, 0, "Active P2", PRIORITY_P2, &out);
	task_model_free(&out);
	storage_task_insert(project_id, 0, "Active P1", PRIORITY_P1, &out);
	task_model_free(&out);
	storage_task_insert(project_id, 0, "Active P3", PRIORITY_P3, &out);
	int64_t completed_id;
	storage_task_insert(project_id, 0, "To complete", PRIORITY_P1, &out);
	completed_id = out.id;
	task_model_free(&out);
	storage_task_set_completed(completed_id, true, false);

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(4, (int)n);
	TEST_ASSERT_EQUAL_STRING("Active P1", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("Active P2", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("Active P3", arr[2].title);
	TEST_ASSERT_EQUAL_STRING("To complete", arr[3].title);
	TEST_ASSERT_EQUAL_INT(TASK_STATE_COMPLETED, arr[3].state);
	storage_task_array_free(arr, n);
}

void test_storage_task_insert_appends_with_spaced_manual_order(void) {
	task_t a, b;
	storage_task_insert(project_id, 0, "A", PRIORITY_P1, &a);
	storage_task_insert(project_id, 0, "B", PRIORITY_P1, &b);

	TEST_ASSERT_EQUAL_INT(10, (int)a.manual_order);
	TEST_ASSERT_EQUAL_INT(20, (int)b.manual_order);

	task_model_free(&a);
	task_model_free(&b);
}

void test_storage_task_reorder_move_swaps_and_stops_at_boundary(void) {
	task_t a, b, c;
	storage_task_insert(project_id, 0, "A", PRIORITY_P3, &a);
	storage_task_insert(project_id, 0, "B", PRIORITY_P3, &b);
	storage_task_insert(project_id, 0, "C", PRIORITY_P3, &c);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_reorder_move(c.id, -1));

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_STRING("A", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("C", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("B", arr[2].title);
	storage_task_array_free(arr, n);

	/* A is first; moving it up further is a no-op boundary. */
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_reorder_move(a.id, -1));

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_storage_task_set_completed_cascades_each_subtask_to_its_own_group(void) {
	task_t parent, sub_p1, sub_p3, existing_completed_p1;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P2, &parent);
	storage_task_insert(project_id, parent.id, "SubP1", PRIORITY_P1, &sub_p1);
	storage_task_insert(project_id, parent.id, "SubP3", PRIORITY_P3, &sub_p3);
	/* A pre-existing completed P1 subtask of the same parent, to verify append-after. */
	storage_task_insert(project_id, parent.id, "AlreadyDoneP1", PRIORITY_P1, &existing_completed_p1);
	storage_task_set_completed(existing_completed_p1.id, true, false);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_set_completed(parent.id, true, true));

	task_t fetched;
	storage_task_get(parent.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	task_t *subs = NULL;
	size_t n = 0;
	storage_task_list_subtasks(parent.id, false, &subs, &n);
	TEST_ASSERT_EQUAL_INT(3, (int)n);
	/* Completed P1 group: AlreadyDoneP1 first (manual_order 10), SubP1 appended after (20). */
	TEST_ASSERT_EQUAL_STRING("AlreadyDoneP1", subs[0].title);
	TEST_ASSERT_EQUAL_STRING("SubP1", subs[1].title);
	TEST_ASSERT_TRUE(subs[1].manual_order > subs[0].manual_order);
	TEST_ASSERT_EQUAL_STRING("SubP3", subs[2].title);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, subs[1].status);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, subs[2].status);
	storage_task_array_free(subs, n);

	task_model_free(&parent);
	task_model_free(&sub_p1);
	task_model_free(&sub_p3);
	task_model_free(&existing_completed_p1);
}

void test_storage_task_archive_completed_scoped_to_project(void) {
	int64_t other_id;
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "other");
	storage_project_insert(&other, &other_id);

	task_t a, b;
	storage_task_insert(project_id, 0, "A", PRIORITY_P1, &a);
	storage_task_insert(other_id, 0, "B", PRIORITY_P1, &b);
	storage_task_set_completed(a.id, true, false);
	storage_task_set_completed(b.id, true, false);

	int count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_archive_completed(project_id, &count, true));
	TEST_ASSERT_EQUAL_INT(1, count);

	task_t fetched;
	storage_task_get(a.id, &fetched);
	TEST_ASSERT_TRUE(fetched.archived);
	task_model_free(&fetched);
	storage_task_get(b.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	task_model_free(&fetched);

	task_model_free(&a);
	task_model_free(&b);
}

void test_storage_project_delete_cascade_removes_tasks_and_subtasks(void) {
	task_t parent, sub;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P3, &parent);
	storage_task_insert(project_id, parent.id, "Sub", PRIORITY_P3, &sub);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_delete_cascade(project_id));

	project_t p;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_project_get(project_id, &p));
	task_t t;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(parent.id, &t));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(sub.id, &t));

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_storage_project_clear_tasks_keeps_project_row(void) {
	task_t a;
	storage_task_insert(project_id, 0, "A", PRIORITY_P3, &a);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_clear_tasks(project_id));

	project_t p;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_get(project_id, &p));
	project_model_free(&p);
	task_t t;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(a.id, &t));

	task_model_free(&a);
}

void test_storage_project_search_escapes_like_wildcards(void) {
	project_t special = {0};
	snprintf(special.display_name, sizeof(special.display_name), "50%%off");
	int64_t special_id;
	storage_project_insert(&special, &special_id);

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_search("50%", false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(1, (int)n);
	TEST_ASSERT_EQUAL_INT64(special_id, arr[0].id);
	storage_project_array_free(arr, n);
}

void test_storage_task_no_double_nesting_trigger_rejects_grandchild(void) {
	task_t parent, sub, grandchild;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P3, &parent);
	storage_task_insert(project_id, parent.id, "Sub", PRIORITY_P3, &sub);

	TEST_ASSERT_EQUAL_INT(RT_ERROR,
		storage_task_insert(project_id, sub.id, "Grandchild", PRIORITY_P3, &grandchild));

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_storage_transaction_rollback_discards_changes(void) {
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_begin());
	task_t out;
	storage_task_insert(project_id, 0, "Rolled back", PRIORITY_P3, &out);
	task_model_free(&out);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_rollback());

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(0, (int)n);
	storage_task_array_free(arr, n);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_storage_open_seeds_builtin_projects);
	RUN_TEST(test_storage_open_seeding_is_idempotent_across_reopens);
	RUN_TEST(test_storage_project_find_by_path_not_found);
	RUN_TEST(test_storage_task_list_top_level_orders_by_state_priority_manual_order);
	RUN_TEST(test_storage_task_insert_appends_with_spaced_manual_order);
	RUN_TEST(test_storage_task_reorder_move_swaps_and_stops_at_boundary);
	RUN_TEST(test_storage_task_set_completed_cascades_each_subtask_to_its_own_group);
	RUN_TEST(test_storage_task_archive_completed_scoped_to_project);
	RUN_TEST(test_storage_project_delete_cascade_removes_tasks_and_subtasks);
	RUN_TEST(test_storage_project_clear_tasks_keeps_project_row);
	RUN_TEST(test_storage_project_search_escapes_like_wildcards);
	RUN_TEST(test_storage_task_no_double_nesting_trigger_rejects_grandchild);
	RUN_TEST(test_storage_transaction_rollback_discards_changes);
	return UNITY_END();
}
