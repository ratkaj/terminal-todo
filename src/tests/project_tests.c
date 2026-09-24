// lspdiag

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <logger.h>
#include <project.h>
#include <storage.h>
#include <task.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_project.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));
}

void tearDown(void) {
	storage_close();
	logger_close();
}

void test_project_create_explicit_success(void) {
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_create_explicit("atomrpc", "/home/user/work/atomrpc", &out));
	TEST_ASSERT_EQUAL_STRING("atomrpc", out.display_name);
	TEST_ASSERT_EQUAL_STRING("/home/user/work/atomrpc", out.canonical_path);
	TEST_ASSERT_FALSE(out.builtin);
	project_model_free(&out);
}

void test_project_create_explicit_without_path(void) {
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_create_explicit("Errands", NULL, &out));
	TEST_ASSERT_NULL(out.canonical_path);
	project_model_free(&out);
}

void test_project_create_explicit_rejects_empty_name(void) {
	TEST_ASSERT_EQUAL_INT(RT_ERROR, project_create_explicit("", NULL, NULL));
}

void test_project_commit_provisional_with_task_persists_both(void) {
	project_t provisional = {0};
	snprintf(provisional.display_name, sizeof(provisional.display_name), "atomrpc");
	provisional.canonical_path = strdup("/home/user/work/atomrpc");

	task_t out_task;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_commit_provisional_with_task(&provisional, "First task", PRIORITY_P3, &out_task));
	TEST_ASSERT_NOT_EQUAL(0, provisional.id);

	project_t fetched;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_get(provisional.id, &fetched));
	TEST_ASSERT_EQUAL_STRING("atomrpc", fetched.display_name);
	project_model_free(&fetched);

	TEST_ASSERT_EQUAL_STRING("First task", out_task.title);

	free(provisional.canonical_path);
	task_model_free(&out_task);
}

void test_project_commit_provisional_with_task_leaves_no_project_on_task_failure(void) {
	project_t provisional = {0};
	snprintf(provisional.display_name, sizeof(provisional.display_name), "atomrpc");

	task_t out_task;
	/* Empty title fails task_create's validation inside the transaction. */
	TEST_ASSERT_EQUAL_INT(RT_ERROR,
		project_commit_provisional_with_task(&provisional, "", PRIORITY_P3, &out_task));

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(true, &arr, &n);
	for (size_t i = 0; i < n; i++)
		TEST_ASSERT_FALSE(strcmp(arr[i].display_name, "atomrpc") == 0);
	storage_project_array_free(arr, n);
}

void test_project_rename_keeps_canonical_path(void) {
	project_t created;
	project_create_explicit("atomrpc", "/home/user/work/atomrpc", &created);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_rename(created.id, "AtomRPC"));

	project_t fetched;
	storage_project_get(created.id, &fetched);
	TEST_ASSERT_EQUAL_STRING("AtomRPC", fetched.display_name);
	TEST_ASSERT_EQUAL_STRING("/home/user/work/atomrpc", fetched.canonical_path);

	project_model_free(&created);
	project_model_free(&fetched);
}

void test_project_archive_rejects_builtin(void) {
	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(true, &arr, &n);
	int64_t today_id = -1;
	for (size_t i = 0; i < n; i++)
		if (strcmp(arr[i].display_name, "Today") == 0)
			today_id = arr[i].id;
	storage_project_array_free(arr, n);
	TEST_ASSERT_NOT_EQUAL(-1, today_id);

	TEST_ASSERT_EQUAL_INT(RT_ERROR, project_archive(today_id));
}

void test_project_archive_and_restore_regular_project(void) {
	project_t created;
	project_create_explicit("atomrpc", NULL, &created);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_archive(created.id));
	project_t fetched;
	storage_project_get(created.id, &fetched);
	TEST_ASSERT_TRUE(fetched.archived);
	project_model_free(&fetched);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_restore(created.id));
	storage_project_get(created.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	project_model_free(&fetched);

	project_model_free(&created);
}

void test_project_delete_or_clear_deletes_regular_project_cascade(void) {
	project_t created;
	project_create_explicit("atomrpc", NULL, &created);
	task_t t;
	task_create(created.id, 0, "A task", PRIORITY_P3, &t);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_delete_or_clear(created.id));

	project_t fetched;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_project_get(created.id, &fetched));
	task_t fetched_task;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(t.id, &fetched_task));

	project_model_free(&created);
	task_model_free(&t);
}

void test_project_delete_or_clear_clears_builtin_but_keeps_it(void) {
	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(true, &arr, &n);
	int64_t inbox_id = -1;
	for (size_t i = 0; i < n; i++)
		if (strcmp(arr[i].display_name, "Inbox") == 0)
			inbox_id = arr[i].id;
	storage_project_array_free(arr, n);

	task_t t;
	task_create(inbox_id, 0, "Inbox task", PRIORITY_P3, &t);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_delete_or_clear(inbox_id));

	project_t fetched;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_get(inbox_id, &fetched));
	project_model_free(&fetched);
	task_t fetched_task;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(t.id, &fetched_task));

	task_model_free(&t);
}

void test_project_resolve_or_provisional_registered_directory(void) {
	char template[] = "/tmp/todo_project_test_XXXXXX";
	char *tmpdir = mkdtemp(template);
	TEST_ASSERT_NOT_NULL(tmpdir);

	char realpathbuf[PATH_MAX];
	TEST_ASSERT_NOT_NULL(realpath(tmpdir, realpathbuf));

	project_t seeded = {0};
	snprintf(seeded.display_name, sizeof(seeded.display_name), "tmp-project");
	seeded.canonical_path = strdup(realpathbuf);
	int64_t seeded_id;
	storage_project_insert(&seeded, &seeded_id);
	free(seeded.canonical_path);

	char oldcwd[PATH_MAX];
	TEST_ASSERT_NOT_NULL(getcwd(oldcwd, sizeof(oldcwd)));
	TEST_ASSERT_EQUAL_INT(0, chdir(tmpdir));

	project_t out;
	bool is_provisional = true;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, project_resolve_or_provisional(&out, &is_provisional));
	TEST_ASSERT_FALSE(is_provisional);
	TEST_ASSERT_EQUAL_INT64(seeded_id, out.id);

	TEST_ASSERT_EQUAL_INT(0, chdir(oldcwd));
	project_model_free(&out);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_project_create_explicit_success);
	RUN_TEST(test_project_create_explicit_without_path);
	RUN_TEST(test_project_create_explicit_rejects_empty_name);
	RUN_TEST(test_project_commit_provisional_with_task_persists_both);
	RUN_TEST(test_project_commit_provisional_with_task_leaves_no_project_on_task_failure);
	RUN_TEST(test_project_rename_keeps_canonical_path);
	RUN_TEST(test_project_archive_rejects_builtin);
	RUN_TEST(test_project_archive_and_restore_regular_project);
	RUN_TEST(test_project_delete_or_clear_deletes_regular_project_cascade);
	RUN_TEST(test_project_delete_or_clear_clears_builtin_but_keeps_it);
	RUN_TEST(test_project_resolve_or_provisional_registered_directory);
	return UNITY_END();
}
