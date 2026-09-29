// lspdiag

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include <common.h>
#include <logger.h>
#include <storage.h>
#include <unity/unity.h>

static int64_t project_id;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_storage.log");
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

void test_storage_task_reorder_move_swaps_tasks_with_tied_manual_order(void) {
	/* Two archive runs give archived A and B the same manual_order. */
	task_t a, b;
	int count;
	storage_task_insert(project_id, 0, "A", PRIORITY_P3, &a);
	storage_task_set_completed(a.id, true, false);
	storage_task_archive_completed(project_id, &count, true);
	storage_task_insert(project_id, 0, "B", PRIORITY_P3, &b);
	storage_task_set_completed(b.id, true, false);
	storage_task_archive_completed(project_id, &count, true);

	task_t fa, fb;
	storage_task_get(a.id, &fa);
	storage_task_get(b.id, &fb);
	TEST_ASSERT_EQUAL_INT((int)fa.manual_order, (int)fb.manual_order);
	task_model_free(&fa);
	task_model_free(&fb);

	/* Ties list by id, so A is first; both directions must swap. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_reorder_move(b.id, -1));
	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, true, &arr, &n);
	TEST_ASSERT_EQUAL_INT(2, (int)n);
	TEST_ASSERT_EQUAL_STRING("B", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("A", arr[1].title);
	storage_task_array_free(arr, n);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_reorder_move(b.id, 1));
	storage_task_list_top_level(project_id, true, &arr, &n);
	TEST_ASSERT_EQUAL_STRING("A", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("B", arr[1].title);
	storage_task_array_free(arr, n);

	/* Still a no-op at the edges. */
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_reorder_move(a.id, -1));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_reorder_move(b.id, 1));

	task_model_free(&a);
	task_model_free(&b);
}

void test_storage_failed_commit_rolls_back_and_later_writes_persist(void) {
	char path[] = "/tmp/todo_storage_test_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);

	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(path));
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "busy");
	int64_t pid = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &pid));
	task_t a, b;
	storage_task_insert(pid, 0, "A", PRIORITY_P3, &a);
	storage_task_insert(pid, 0, "B", PRIORITY_P3, &b);

	/* A second connection mid-read holds a shared lock, so COMMIT cannot
	   get the exclusive lock and fails with SQLITE_BUSY after the timeout. */
	sqlite3 *reader = NULL;
	sqlite3_stmt *stmt = NULL;
	TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(path, &reader));
	TEST_ASSERT_EQUAL_INT(SQLITE_OK,
		sqlite3_prepare_v2(reader, "SELECT id FROM task", -1, &stmt, NULL));
	TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(stmt));

	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_reorder_move(b.id, -1));

	sqlite3_finalize(stmt);
	sqlite3_close(reader);

	/* The failed transaction was rolled back, so a new one can start and
	   the reorder did not take effect. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_begin());
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_rollback());

	task_t c;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_insert(pid, 0, "C", PRIORITY_P3, &c));
	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(path));

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(pid, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(3, (int)n);
	TEST_ASSERT_EQUAL_STRING("A", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("B", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("C", arr[2].title);
	storage_task_array_free(arr, n);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
	unlink(path);
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

void test_storage_project_search_ignores_case_beyond_ascii(void) {
	/* towlower() maps non-ASCII only in a UTF-8 LC_CTYPE, as the app runs. */
	TEST_ASSERT_NOT_NULL(setlocale(LC_CTYPE, "C.UTF-8"));
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "Čvor Šuma");
	int64_t id;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));

	const char *queries[] = { "čvor", "ČVOR", "šuma", "r š", "" };
	for (size_t q = 0; q < sizeof(queries) / sizeof(queries[0]); q++) {
		project_t *arr = NULL;
		size_t n = 0;
		TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_search(queries[q], false, &arr, &n));
		bool found = false;
		for (size_t i = 0; i < n; i++)
			found = found || arr[i].id == id;
		storage_project_array_free(arr, n);
		TEST_ASSERT_TRUE_MESSAGE(found, queries[q]);
	}

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_search("cvor", false, &arr, &n); /* accents still count */
	TEST_ASSERT_EQUAL_INT(0, (int)n);
	storage_project_array_free(arr, n);
	setlocale(LC_CTYPE, "C");
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

void test_storage_project_count_archived_counts_only_archived(void) {
	int64_t other_id;
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "other");
	storage_project_insert(&other, &other_id);

	TEST_ASSERT_EQUAL_INT(0, storage_project_count_archived());

	project_t p;
	storage_project_get(project_id, &p);
	p.archived = true;
	storage_project_update(&p);
	project_model_free(&p);

	TEST_ASSERT_EQUAL_INT(1, storage_project_count_archived());
}

void test_storage_task_count_archived_scoped_to_project_includes_subtasks(void) {
	int64_t other_id;
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "other");
	storage_project_insert(&other, &other_id);

	task_t parent, sub, unrelated;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P3, &parent);
	storage_task_insert(project_id, parent.id, "Sub", PRIORITY_P3, &sub);
	storage_task_insert(other_id, 0, "Unrelated", PRIORITY_P3, &unrelated);

	storage_task_set_completed(parent.id, true, true);
	int count = 0;
	storage_task_archive_completed(project_id, &count, true);
	storage_task_set_completed(unrelated.id, true, false);
	int other_count = 0;
	storage_task_archive_completed(other_id, &other_count, true);

	TEST_ASSERT_EQUAL_INT(2, storage_task_count_archived(project_id));
	TEST_ASSERT_EQUAL_INT(1, storage_task_count_archived(other_id));

	task_model_free(&parent);
	task_model_free(&sub);
	task_model_free(&unrelated);
}

static int64_t insert_named_project(const char *name, bool archived) {
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "%s", name);
	int64_t id;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));
	if (archived) {
		p.id = id;
		p.archived = true;
		TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_update(&p));
	}
	return id;
}

void test_storage_task_move_project_moves_task_and_subtasks(void) {
	int64_t dest = insert_named_project("panzerpi", false);
	task_t parent, sub, existing;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P2, &parent);
	storage_task_insert(project_id, parent.id, "Sub", PRIORITY_P3, &sub);
	storage_task_insert(dest, 0, "Already there", PRIORITY_P2, &existing);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_move_project(parent.id, dest));

	task_t moved, moved_sub;
	storage_task_get(parent.id, &moved);
	storage_task_get(sub.id, &moved_sub);
	TEST_ASSERT_EQUAL_INT64(dest, moved.project_id);
	TEST_ASSERT_EQUAL_INT64(dest, moved_sub.project_id);
	TEST_ASSERT_EQUAL_INT64(parent.id, moved_sub.parent_id);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P2, moved.priority);
	/* Appended after the destination's existing P2 task. */
	TEST_ASSERT_TRUE(moved.manual_order > existing.manual_order);
	TEST_ASSERT_EQUAL_INT(0, storage_project_task_count(project_id));

	task_model_free(&parent);
	task_model_free(&sub);
	task_model_free(&existing);
	task_model_free(&moved);
	task_model_free(&moved_sub);
}

void test_storage_task_move_project_keeps_completed_state_and_group(void) {
	int64_t dest = insert_named_project("panzerpi", false);
	task_t t, done_there;
	storage_task_insert(project_id, 0, "Done here", PRIORITY_P3, &t);
	storage_task_set_completed(t.id, true, false);
	storage_task_insert(dest, 0, "Done there", PRIORITY_P3, NULL);
	storage_task_insert(dest, 0, "Open there", PRIORITY_P3, NULL);

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(dest, false, &arr, &n);
	TEST_ASSERT_EQUAL_size_t(2, n);
	storage_task_set_completed(arr[0].id, true, false);
	storage_task_get(arr[0].id, &done_there);
	storage_task_array_free(arr, n);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_move_project(t.id, dest));

	task_t moved;
	storage_task_get(t.id, &moved);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, moved.status);
	TEST_ASSERT_TRUE(moved.manual_order > done_there.manual_order);

	storage_task_list_top_level(dest, false, &arr, &n);
	TEST_ASSERT_EQUAL_size_t(3, n);
	TEST_ASSERT_EQUAL_INT64(t.id, arr[2].id);
	storage_task_array_free(arr, n);

	task_model_free(&t);
	task_model_free(&done_there);
	task_model_free(&moved);
}

void test_storage_project_list_move_targets_excludes_current_and_archived(void) {
	int64_t active = insert_named_project("panzerpi", false);
	int64_t archived = insert_named_project("old", true);

	project_t *arr = NULL;
	size_t n = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_list_move_targets(project_id, &arr, &n));
	bool saw_active = false;
	for (size_t i = 0; i < n; i++) {
		TEST_ASSERT_NOT_EQUAL(project_id, arr[i].id);
		TEST_ASSERT_NOT_EQUAL(archived, arr[i].id);
		if (arr[i].id == active)
			saw_active = true;
	}
	/* Three built-ins first, then the one other active project. */
	TEST_ASSERT_EQUAL_size_t(4, n);
	TEST_ASSERT_TRUE(arr[0].builtin);
	TEST_ASSERT_TRUE(saw_active);
	storage_project_array_free(arr, n);
}

void test_storage_task_list_completed_between_bounds_and_parent_context(void) {
	task_t parent, sub, done, open_task;
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P1, &parent);
	storage_task_insert(project_id, parent.id, "Sub", PRIORITY_P3, &sub);
	storage_task_insert(project_id, 0, "Done", PRIORITY_P2, &done);
	storage_task_insert(project_id, 0, "Open", PRIORITY_P3, &open_task);
	storage_task_set_completed(sub.id, true, false);
	storage_task_set_completed(done.id, true, false);

	time_t now = time(NULL);
	task_t *arr = NULL;
	size_t n = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		storage_task_list_completed_between(now - 60, now + 60, &arr, &n));
	/* Both completions, plus the open parent for context, parent before sub. */
	TEST_ASSERT_EQUAL_size_t(3, n);
	size_t pi = n, si = n;
	for (size_t i = 0; i < n; i++) {
		TEST_ASSERT_NOT_EQUAL(open_task.id, arr[i].id);
		if (arr[i].id == parent.id) pi = i;
		if (arr[i].id == sub.id) si = i;
	}
	TEST_ASSERT_TRUE(pi < si && si < n);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_OPEN, arr[pi].status);
	storage_task_array_free(arr, n);

	/* End is exclusive: a range ending before the completions is empty. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		storage_task_list_completed_between(now - 3600, now - 60, &arr, &n));
	TEST_ASSERT_EQUAL_size_t(0, n);
	storage_task_array_free(arr, n);

	task_model_free(&parent);
	task_model_free(&sub);
	task_model_free(&done);
	task_model_free(&open_task);
}

void test_storage_task_search_ranks_title_matches_above_notes_only(void) {
	task_t a, b, c;
	storage_task_insert(project_id, 0, "Write docs", PRIORITY_P3, &a);
	storage_task_insert(project_id, 0, "Investigate broker reconnect", PRIORITY_P3, &b);
	storage_task_insert(project_id, 0, "Unrelated", PRIORITY_P3, &c);
	storage_task_update_fields(a.id, NULL, "The BROKER drops after 30s");

	task_search_hit_t *hits = NULL;
	size_t n = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_search("broker", 50, &hits, &n));
	TEST_ASSERT_EQUAL_size_t(2, n);
	TEST_ASSERT_EQUAL_INT64(b.id, hits[0].task.id);   /* title match */
	TEST_ASSERT_EQUAL_INT64(a.id, hits[1].task.id);   /* notes only */
	TEST_ASSERT_EQUAL_STRING("atomrpc", hits[0].project_name);
	TEST_ASSERT_EQUAL_STRING("", hits[0].parent_title);
	storage_task_search_free(hits, n);

	/* Notes need whole words: a subsequence is not enough there. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_search("brkr", 50, &hits, &n));
	TEST_ASSERT_EQUAL_size_t(1, n);
	TEST_ASSERT_EQUAL_INT64(b.id, hits[0].task.id);
	storage_task_search_free(hits, n);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_storage_task_search_includes_archived_tasks_and_projects(void) {
	int64_t old = insert_named_project("oldproj", true);
	task_t parent, sub, gone;
	storage_task_insert(project_id, 0, "MQTT parent", PRIORITY_P3, &parent);
	storage_task_insert(project_id, parent.id, "MQTT sub", PRIORITY_P1, &sub);
	storage_task_insert(old, 0, "MQTT in old project", PRIORITY_P3, &gone);
	storage_task_set_completed(parent.id, true, true);
	int count = 0;
	storage_task_archive_completed(project_id, &count, true);

	task_search_hit_t *hits = NULL;
	size_t n = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_search("mqtt", 50, &hits, &n));
	TEST_ASSERT_EQUAL_size_t(3, n);
	bool saw_sub = false, saw_old = false;
	for (size_t i = 0; i < n; i++) {
		if (hits[i].task.id == sub.id) {
			saw_sub = true;
			TEST_ASSERT_TRUE(hits[i].task.archived);
			TEST_ASSERT_EQUAL_INT(TASK_STATE_ARCHIVED, hits[i].task.state);
			TEST_ASSERT_EQUAL_STRING("MQTT parent", hits[i].parent_title);
			TEST_ASSERT_TRUE(hits[i].parent_archived);
		}
		if (hits[i].task.id == gone.id) {
			saw_old = true;
			TEST_ASSERT_TRUE(hits[i].project_archived);
			TEST_ASSERT_EQUAL_STRING("oldproj", hits[i].project_name);
		}
	}
	TEST_ASSERT_TRUE(saw_sub);
	TEST_ASSERT_TRUE(saw_old);
	storage_task_search_free(hits, n);

	task_model_free(&parent);
	task_model_free(&sub);
	task_model_free(&gone);
}

void test_storage_task_search_blank_query_and_limit(void) {
	task_t t;
	for (int i = 0; i < 5; i++) {
		storage_task_insert(project_id, 0, "Same title", PRIORITY_P3, &t);
		task_model_free(&t);
	}
	task_search_hit_t *hits = NULL;
	size_t n = 99;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_search("   ", 50, &hits, &n));
	TEST_ASSERT_EQUAL_size_t(0, n);
	TEST_ASSERT_NULL(hits);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_search("same", 3, &hits, &n));
	TEST_ASSERT_EQUAL_size_t(3, n);
	storage_task_search_free(hits, n);
}

/* ---- undo ---- */

static void dump_task(char *out, size_t cap, const task_t *t) {
	size_t len = strlen(out);
	snprintf(out + len, cap - len, "T%lld p%lld par%lld '%s' n'%s' s%d pr%d o%ld a%d c%lld d%lld\n",
		(long long)t->id, (long long)t->project_id, (long long)t->parent_id, t->title,
		t->notes ? t->notes : "(null)", (int)t->status, (int)t->priority, t->manual_order,
		(int)t->archived, (long long)t->created_at, (long long)t->completed_at);
}

/* Every project and task, every column, in a stable order. */
static void dump_db(char *out, size_t cap) {
	out[0] = '\0';
	project_t *ps = NULL;
	size_t np = 0;
	storage_project_list(true, &ps, &np);
	for (size_t i = 0; i < np; i++) {
		size_t len = strlen(out);
		snprintf(out + len, cap - len, "P%lld '%s' '%s' a%d b%d\n", (long long)ps[i].id,
			ps[i].display_name, ps[i].canonical_path ? ps[i].canonical_path : "(null)",
			(int)ps[i].archived, (int)ps[i].builtin);
		task_t *ts = NULL;
		size_t nt = 0;
		storage_task_list_top_level(ps[i].id, true, &ts, &nt);
		for (size_t j = 0; j < nt; j++) {
			dump_task(out, cap, &ts[j]);
			task_t *subs = NULL;
			size_t ns = 0;
			storage_task_list_subtasks(ts[j].id, true, &subs, &ns);
			for (size_t k = 0; k < ns; k++)
				dump_task(out, cap, &subs[k]);
			storage_task_array_free(subs, ns);
		}
		storage_task_array_free(ts, nt);
	}
	storage_project_array_free(ps, np);
}

#define DUMP_CAP 8192

/* A parent with notes and two subtasks, one completed, plus a sibling. */
static void seed_block(task_t *parent, task_t *sub1, task_t *sub2, task_t *other) {
	storage_task_insert(project_id, 0, "Parent", PRIORITY_P2, parent);
	storage_task_insert(project_id, parent->id, "Sub one", PRIORITY_P1, sub1);
	storage_task_insert(project_id, parent->id, "Sub two", PRIORITY_P3, sub2);
	storage_task_insert(project_id, 0, "Other", PRIORITY_P3, other);
	storage_task_update_fields(parent->id, NULL, "Some 'quoted' notes\nline two");
	storage_task_set_completed(sub1->id, true, false);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_checkpoint());
}

static void free_block(task_t *a, task_t *b, task_t *c, task_t *d) {
	task_model_free(a);
	task_model_free(b);
	task_model_free(c);
	task_model_free(d);
}

/* Snapshot, run @p op as one step, undo it, and require the exact snapshot back. */
#define ASSERT_UNDO_RESTORES(op) do {                                        \
	static char before[DUMP_CAP], after[DUMP_CAP];                            \
	dump_db(before, sizeof(before));                                          \
	op;                                                                       \
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_checkpoint());             \
	dump_db(after, sizeof(after));                                            \
	TEST_ASSERT_TRUE(strcmp(before, after) != 0);                             \
	storage_undo_info_t info;                                                 \
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_last(&info));              \
	dump_db(after, sizeof(after));                                            \
	TEST_ASSERT_EQUAL_STRING(before, after);                                  \
} while (0)

void test_storage_undo_restores_deleted_parent_with_subtasks_and_notes(void) {
	task_t parent, sub1, sub2, other;
	seed_block(&parent, &sub1, &sub2, &other);
	ASSERT_UNDO_RESTORES(storage_task_delete_cascade(parent.id));
	free_block(&parent, &sub1, &sub2, &other);
}

void test_storage_undo_restores_deleted_project_and_cleared_builtin(void) {
	task_t parent, sub1, sub2, other;
	seed_block(&parent, &sub1, &sub2, &other);
	ASSERT_UNDO_RESTORES(storage_project_delete_cascade(project_id));

	int64_t inbox = find_builtin_id("Inbox");
	task_t t;
	storage_task_insert(inbox, 0, "Inbox task", PRIORITY_P3, &t);
	storage_undo_checkpoint();
	ASSERT_UNDO_RESTORES(storage_project_clear_tasks(inbox));
	task_model_free(&t);
	free_block(&parent, &sub1, &sub2, &other);
}

void test_storage_undo_restores_archive_restore_move_and_edits(void) {
	task_t parent, sub1, sub2, other;
	seed_block(&parent, &sub1, &sub2, &other);
	int64_t dest = insert_named_project("panzerpi", false);
	storage_undo_checkpoint();
	int count = 0;

	ASSERT_UNDO_RESTORES(storage_task_set_completed(parent.id, true, true));
	storage_task_set_completed(parent.id, true, true);
	storage_undo_checkpoint();
	ASSERT_UNDO_RESTORES(storage_task_archive_completed(project_id, &count, true));
	storage_task_archive_completed(project_id, &count, true);
	storage_undo_checkpoint();
	ASSERT_UNDO_RESTORES(storage_task_restore(parent.id));
	storage_task_restore(parent.id);
	storage_undo_checkpoint();
	ASSERT_UNDO_RESTORES(storage_task_move_project(other.id, dest));
	ASSERT_UNDO_RESTORES(storage_task_set_priority(other.id, PRIORITY_P1));
	ASSERT_UNDO_RESTORES(storage_task_update_fields(other.id, "Renamed", "new notes"));

	project_t p;
	storage_project_get(dest, &p);
	snprintf(p.display_name, sizeof(p.display_name), "renamed");
	p.archived = true;
	ASSERT_UNDO_RESTORES(storage_project_update(&p));
	project_model_free(&p);
	free_block(&parent, &sub1, &sub2, &other);
}

void test_storage_undo_describes_step_and_ignores_unchanged_updates(void) {
	task_t parent, sub1, sub2, other;
	seed_block(&parent, &sub1, &sub2, &other);

	storage_task_delete_cascade(parent.id);
	storage_undo_checkpoint();
	/* Same values: logged as nothing, so no new step is added. */
	storage_task_update_fields(other.id, "Other", NULL);
	storage_task_set_priority(other.id, PRIORITY_P3);
	storage_undo_checkpoint();

	storage_undo_info_t info;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_peek(&info));
	TEST_ASSERT_FALSE(info.is_project);
	TEST_ASSERT_EQUAL_INT64(parent.id, info.row_id);
	TEST_ASSERT_EQUAL_STRING("delete task \"Parent\"", info.label);
	TEST_ASSERT_EQUAL_INT(3, info.changes);
	free_block(&parent, &sub1, &sub2, &other);
}

void test_storage_undo_keeps_only_the_newest_steps(void) {
	task_t t;
	for (int i = 0; i < STORAGE_UNDO_DEPTH + 1; i++) {
		char title[16];
		snprintf(title, sizeof(title), "T%d", i);
		storage_task_insert(project_id, 0, title, PRIORITY_P3, &t);
		task_model_free(&t);
		storage_undo_checkpoint();
	}
	storage_undo_info_t info;
	for (int i = 0; i < STORAGE_UNDO_DEPTH; i++) {
		/* An empty checkpoint in between must not trim a real step. */
		TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_checkpoint());
		TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_last(&info));
	}
	TEST_ASSERT_EQUAL_STRING("create task \"T1\"", info.label);
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_undo_last(&info));
	TEST_ASSERT_TRUE(info.nothing);
	TEST_ASSERT_EQUAL_INT(1, storage_project_task_count(project_id)); /* T0 stays */
}

void test_storage_undo_nothing_on_empty_history(void) {
	/* A fresh connection: setUp's project insert was logged on the old one,
	   and seeding the built-in projects is never logged. */
	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));
	storage_undo_info_t info;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_undo_peek(&info));
	TEST_ASSERT_TRUE(info.nothing);
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_undo_last(&info));
	TEST_ASSERT_TRUE(info.nothing);
}

void test_storage_undo_failed_replay_rolls_back_and_drops_the_step(void) {
	char path[] = "/tmp/todo_undo_test_XXXXXX";
	int fd = mkstemp(path);
	TEST_ASSERT_TRUE(fd >= 0);
	close(fd);
	storage_close();
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(path));

	int64_t other_proj = insert_named_project("other", false);
	task_t keep, doomed;
	storage_task_insert(other_proj, 0, "Keep", PRIORITY_P3, &keep);
	storage_task_insert(other_proj, 0, "Doomed", PRIORITY_P3, &doomed);
	storage_undo_checkpoint();
	storage_task_delete_cascade(doomed.id);
	storage_undo_checkpoint();

	/* Another instance deletes the project the step restores into. */
	sqlite3 *other = NULL;
	TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(path, &other));
	char sql[128];
	snprintf(sql, sizeof(sql), "PRAGMA foreign_keys=ON; DELETE FROM project WHERE id=%lld;",
		(long long)other_proj);
	TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(other, sql, NULL, NULL, NULL));
	sqlite3_close(other);

	storage_undo_info_t info;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_undo_last(&info));
	TEST_ASSERT_FALSE(info.nothing);
	task_t probe;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(doomed.id, &probe));
	/* The failed step is gone; the one before it (creating the project and
	   its tasks, named by the project) is next. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_undo_peek(&info));
	TEST_ASSERT_EQUAL_STRING("create project \"other\"", info.label);

	task_model_free(&keep);
	task_model_free(&doomed);
	storage_close();
	unlink(path);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_storage_open_seeds_builtin_projects);
	RUN_TEST(test_storage_open_seeding_is_idempotent_across_reopens);
	RUN_TEST(test_storage_project_find_by_path_not_found);
	RUN_TEST(test_storage_task_list_top_level_orders_by_state_priority_manual_order);
	RUN_TEST(test_storage_task_insert_appends_with_spaced_manual_order);
	RUN_TEST(test_storage_task_reorder_move_swaps_and_stops_at_boundary);
	RUN_TEST(test_storage_task_reorder_move_swaps_tasks_with_tied_manual_order);
	RUN_TEST(test_storage_failed_commit_rolls_back_and_later_writes_persist);
	RUN_TEST(test_storage_task_set_completed_cascades_each_subtask_to_its_own_group);
	RUN_TEST(test_storage_task_archive_completed_scoped_to_project);
	RUN_TEST(test_storage_project_delete_cascade_removes_tasks_and_subtasks);
	RUN_TEST(test_storage_project_clear_tasks_keeps_project_row);
	RUN_TEST(test_storage_project_search_escapes_like_wildcards);
	RUN_TEST(test_storage_project_search_ignores_case_beyond_ascii);
	RUN_TEST(test_storage_task_no_double_nesting_trigger_rejects_grandchild);
	RUN_TEST(test_storage_transaction_rollback_discards_changes);
	RUN_TEST(test_storage_project_count_archived_counts_only_archived);
	RUN_TEST(test_storage_task_count_archived_scoped_to_project_includes_subtasks);
	RUN_TEST(test_storage_task_move_project_moves_task_and_subtasks);
	RUN_TEST(test_storage_task_move_project_keeps_completed_state_and_group);
	RUN_TEST(test_storage_project_list_move_targets_excludes_current_and_archived);
	RUN_TEST(test_storage_task_list_completed_between_bounds_and_parent_context);
	RUN_TEST(test_storage_task_search_ranks_title_matches_above_notes_only);
	RUN_TEST(test_storage_task_search_includes_archived_tasks_and_projects);
	RUN_TEST(test_storage_task_search_blank_query_and_limit);
	RUN_TEST(test_storage_undo_restores_deleted_parent_with_subtasks_and_notes);
	RUN_TEST(test_storage_undo_restores_deleted_project_and_cleared_builtin);
	RUN_TEST(test_storage_undo_restores_archive_restore_move_and_edits);
	RUN_TEST(test_storage_undo_describes_step_and_ignores_unchanged_updates);
	RUN_TEST(test_storage_undo_keeps_only_the_newest_steps);
	RUN_TEST(test_storage_undo_nothing_on_empty_history);
	RUN_TEST(test_storage_undo_failed_replay_rolls_back_and_drops_the_step);
	return UNITY_END();
}
