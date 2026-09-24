// lspdiag

#include <string.h>

#include <common.h>
#include <logger.h>
#include <storage.h>
#include <task.h>
#include <unity/unity.h>

static int64_t project_id;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_task.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "atomrpc");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &project_id));
}

void tearDown(void) {
	storage_close();
	logger_close();
}

void test_task_create_success(void) {
	task_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		task_create(project_id, 0, "Implement project discovery", PRIORITY_P1, &out));
	TEST_ASSERT_EQUAL_STRING("Implement project discovery", out.title);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P1, out.priority);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_OPEN, out.status);
	TEST_ASSERT_FALSE(out.archived);
	task_model_free(&out);
}

void test_task_create_rejects_empty_title(void) {
	task_t out;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_create(project_id, 0, "", PRIORITY_P3, &out));
}

void test_task_create_rejects_invalid_priority(void) {
	task_t out;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_create(project_id, 0, "x", (priority_t)9, &out));
}

void test_task_create_subtask_defaults_p3(void) {
	task_t parent;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_create(project_id, 0, "Parent", PRIORITY_P1, &parent));

	task_t sub;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub));
	TEST_ASSERT_EQUAL_INT(PRIORITY_P3, sub.priority);
	TEST_ASSERT_EQUAL_INT64(parent.id, sub.parent_id);

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_task_create_rejects_nesting_a_subtask_under_a_subtask(void) {
	task_t parent, sub, grandchild;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_create(project_id, 0, "Parent", PRIORITY_P3, &parent));
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub));

	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_create(project_id, sub.id, "Grandchild", PRIORITY_P3, &grandchild));

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_task_update_fields_title_only_leaves_notes(void) {
	task_t t;
	task_create(project_id, 0, "Original", PRIORITY_P3, &t);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_update_fields(t.id, NULL, "Some notes"));
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_update_fields(t.id, "Renamed", NULL));

	task_t fetched;
	storage_task_get(t.id, &fetched);
	TEST_ASSERT_EQUAL_STRING("Renamed", fetched.title);
	TEST_ASSERT_EQUAL_STRING("Some notes", fetched.notes);

	task_model_free(&t);
	task_model_free(&fetched);
}

void test_task_set_priority_moves_to_end_of_new_group(void) {
	task_t a, b, c;
	task_create(project_id, 0, "A", PRIORITY_P2, &a);
	task_create(project_id, 0, "B", PRIORITY_P2, &b);
	task_create(project_id, 0, "C", PRIORITY_P1, &c);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_priority(a.id, PRIORITY_P1));

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(3, (int)n);
	/* P1 group should now be C then A (A appended after existing C). */
	TEST_ASSERT_EQUAL_STRING("C", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("A", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("B", arr[2].title);
	storage_task_array_free(arr, n);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_task_set_completed_without_subtasks_toggles_immediately(void) {
	task_t t;
	task_create(project_id, 0, "Solo", PRIORITY_P3, &t);

	int subtask_count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(t.id, true, false, &subtask_count));
	TEST_ASSERT_EQUAL_INT(0, subtask_count);

	task_t fetched;
	storage_task_get(t.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);

	task_model_free(&t);
	task_model_free(&fetched);
}

void test_task_set_completed_skips_confirmation_when_subtasks_already_match(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub);
	int subtask_count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(sub.id, true, false, &subtask_count));

	/* The only subtask is already done, so completing the parent needs no prompt. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(parent.id, true, false, &subtask_count));
	TEST_ASSERT_EQUAL_INT(0, subtask_count);

	/* Un-completing it does change the subtask, so that still asks. */
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_set_completed(parent.id, false, false, &subtask_count));
	TEST_ASSERT_EQUAL_INT(1, subtask_count);

	task_t fetched;
	storage_task_get(parent.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_task_set_completed_with_subtasks_requires_confirmation(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub);

	int subtask_count = -1;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_set_completed(parent.id, true, false, &subtask_count));
	TEST_ASSERT_EQUAL_INT(1, subtask_count);

	task_t fetched;
	storage_task_get(parent.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_OPEN, fetched.status);
	task_model_free(&fetched);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(parent.id, true, true, &subtask_count));
	storage_task_get(parent.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);
	storage_task_get(sub.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_task_delete_cascades_to_subtasks(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_delete(parent.id));

	task_t fetched;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(parent.id, &fetched));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, storage_task_get(sub.id, &fetched));

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_task_delete_subtask_leaves_parent_and_siblings(void) {
	task_t parent, sub1, sub2;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child1", PRIORITY_P3, &sub1);
	task_create(project_id, parent.id, "Child2", PRIORITY_P3, &sub2);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_delete(sub1.id));

	task_t fetched;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_get(parent.id, &fetched));
	task_model_free(&fetched);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_get(sub2.id, &fetched));
	task_model_free(&fetched);

	task_model_free(&parent);
	task_model_free(&sub1);
	task_model_free(&sub2);
}

void test_task_clear_notes_leaves_task_intact(void) {
	task_t t;
	task_create(project_id, 0, "Has notes", PRIORITY_P3, &t);
	task_update_fields(t.id, NULL, "Some notes");

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_clear_notes(t.id));

	task_t fetched;
	storage_task_get(t.id, &fetched);
	TEST_ASSERT_NULL(fetched.notes);
	TEST_ASSERT_EQUAL_STRING("Has notes", fetched.title);

	task_model_free(&t);
	task_model_free(&fetched);
}

void test_task_reorder_step_moves_within_group_and_stops_at_boundary(void) {
	task_t a, b, c;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);
	task_create(project_id, 0, "C", PRIORITY_P3, &c);

	/* A B C -> move B up -> B A C */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_reorder_step(b.id, -1));

	task_t *arr = NULL;
	size_t n = 0;
	storage_task_list_top_level(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_STRING("B", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("A", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("C", arr[2].title);
	storage_task_array_free(arr, n);

	/* B is now first; moving up again is a boundary no-op. */
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_reorder_step(b.id, -1));

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_task_archive_completed_counts_and_applies_to_current_project_only(void) {
	int64_t other_project_id;
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "other");
	storage_project_insert(&other, &other_project_id);

	task_t a, b, other_task;
	task_create(project_id, 0, "A", PRIORITY_P1, &a);
	task_create(project_id, 0, "B", PRIORITY_P2, &b);
	task_create(other_project_id, 0, "Other", PRIORITY_P1, &other_task);

	int subtask_count;
	task_set_completed(a.id, true, false, &subtask_count);
	task_set_completed(other_task.id, true, false, &subtask_count);

	int count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_archive_completed(project_id, &count, false));
	TEST_ASSERT_EQUAL_INT(1, count);

	task_t fetched;
	storage_task_get(a.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	task_model_free(&fetched);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_archive_completed(project_id, &count, true));
	TEST_ASSERT_EQUAL_INT(1, count);

	storage_task_get(a.id, &fetched);
	TEST_ASSERT_TRUE(fetched.archived);
	task_model_free(&fetched);

	storage_task_get(other_task.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	task_model_free(&fetched);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&other_task);
}

void test_task_archive_completed_noop_when_none_eligible(void) {
	int count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_archive_completed(project_id, &count, true));
	TEST_ASSERT_EQUAL_INT(0, count);
}

void test_task_restore_clears_only_archived_flag(void) {
	task_t t;
	task_create(project_id, 0, "Task", PRIORITY_P1, &t);
	int subtask_count;
	task_set_completed(t.id, true, false, &subtask_count);
	int count;
	task_archive_completed(project_id, &count, true);

	task_t fetched;
	storage_task_get(t.id, &fetched);
	TEST_ASSERT_TRUE(fetched.archived);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_restore(t.id));

	storage_task_get(t.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P1, fetched.priority);
	task_model_free(&fetched);

	task_model_free(&t);
}

static task_t get_task(int64_t id) {
	task_t t;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_task_get(id, &t));
	return t;
}

void test_task_set_completed_rejects_archived_task(void) {
	task_t p, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P1, &p);
	task_create(project_id, p.id, "Sub", PRIORITY_P1, &sub);
	int count;
	task_set_completed(p.id, true, true, &count);
	task_archive_completed(project_id, &count, true);

	count = -1;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_set_completed(p.id, false, true, &count));
	TEST_ASSERT_EQUAL_INT(0, count); /* no prompt for an archived task */

	task_t fp = get_task(p.id), fs = get_task(sub.id);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fp.status);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fs.status);
	TEST_ASSERT_TRUE(fp.completed_at != 0);
	TEST_ASSERT_TRUE(fs.completed_at != 0);
	task_model_free(&fp);
	task_model_free(&fs);
	task_model_free(&p);
	task_model_free(&sub);
}

void test_task_set_completed_cascade_skips_archived_subtasks(void) {
	task_t p, done, open;
	task_create(project_id, 0, "Parent", PRIORITY_P1, &p);
	task_create(project_id, p.id, "Done", PRIORITY_P1, &done);
	task_create(project_id, p.id, "Open", PRIORITY_P1, &open);
	int count;
	task_set_completed(done.id, true, false, &count);
	task_archive_completed(project_id, &count, true); /* archives only Done */

	count = -1;
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_set_completed(p.id, true, false, &count));
	TEST_ASSERT_EQUAL_INT(1, count); /* prompt counts only the non-archived subtask */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(p.id, true, true, &count));
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_set_completed(p.id, false, true, &count));

	task_t fd = get_task(done.id), fo = get_task(open.id);
	TEST_ASSERT_TRUE(fd.archived);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fd.status);
	TEST_ASSERT_TRUE(fd.completed_at != 0);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_OPEN, fo.status);
	task_model_free(&fd);
	task_model_free(&fo);
	task_model_free(&p);
	task_model_free(&done);
	task_model_free(&open);
}

void test_task_archive_completed_archives_open_subtasks_with_parent(void) {
	task_t p, s1, s2;
	task_create(project_id, 0, "Parent", PRIORITY_P1, &p);
	task_create(project_id, p.id, "S1", PRIORITY_P1, &s1);
	task_create(project_id, p.id, "S2", PRIORITY_P1, &s2);
	int count;
	task_set_completed(p.id, true, true, &count);
	task_set_completed(s2.id, false, false, &count); /* S2 reopened */

	count = -1;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_archive_completed(project_id, &count, false));
	TEST_ASSERT_EQUAL_INT(3, count); /* the prompt count includes open S2 */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_archive_completed(project_id, &count, true));

	int64_t ids[] = { p.id, s1.id, s2.id };
	for (size_t i = 0; i < 3; i++) {
		task_t t = get_task(ids[i]);
		TEST_ASSERT_TRUE(t.archived);
		task_model_free(&t);
	}
	task_t f2 = get_task(s2.id);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_OPEN, f2.status); /* archiving keeps completion */
	task_model_free(&f2);

	task_model_free(&p);
	task_model_free(&s1);
	task_model_free(&s2);
}

void test_task_restore_keeps_parent_and_subtasks_together(void) {
	task_t p, s1, s2;
	task_create(project_id, 0, "Parent", PRIORITY_P1, &p);
	task_create(project_id, p.id, "S1", PRIORITY_P1, &s1);
	task_create(project_id, p.id, "S2", PRIORITY_P1, &s2);
	int count;
	task_set_completed(p.id, true, true, &count);
	task_archive_completed(project_id, &count, true);

	/* Restoring a subtask brings back its parent, not its siblings. */
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_restore(s1.id));
	task_t fp = get_task(p.id), f1 = get_task(s1.id), f2 = get_task(s2.id);
	TEST_ASSERT_FALSE(fp.archived);
	TEST_ASSERT_FALSE(f1.archived);
	TEST_ASSERT_TRUE(f2.archived);
	task_model_free(&fp);
	task_model_free(&f1);
	task_model_free(&f2);

	/* Archive the block again, then restoring the parent brings back all. */
	task_archive_completed(project_id, &count, true);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_restore(p.id));
	int64_t ids[] = { p.id, s1.id, s2.id };
	for (size_t i = 0; i < 3; i++) {
		task_t t = get_task(ids[i]);
		TEST_ASSERT_FALSE(t.archived);
		TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, t.status);
		task_model_free(&t);
	}

	task_model_free(&p);
	task_model_free(&s1);
	task_model_free(&s2);
}

void test_task_list_visible_rows_interleaves_subtasks_under_their_parent(void) {
	task_t p1, p2, s1, s2;
	task_create(project_id, 0, "P1", PRIORITY_P2, &p1);
	task_create(project_id, 0, "P2", PRIORITY_P1, &p2);
	task_create(project_id, p1.id, "S1", PRIORITY_P3, &s1);
	task_create(project_id, p1.id, "S2", PRIORITY_P3, &s2);

	task_t *arr = NULL;
	size_t n = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_list_visible_rows(project_id, false, &arr, &n));
	TEST_ASSERT_EQUAL_INT(4, (int)n);
	/* P2 is P1 priority, sorts first among top-level; P1's subtasks follow P1 immediately. */
	TEST_ASSERT_EQUAL_STRING("P2", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("P1", arr[1].title);
	TEST_ASSERT_EQUAL_STRING("S1", arr[2].title);
	TEST_ASSERT_EQUAL_STRING("S2", arr[3].title);

	storage_task_array_free(arr, n);
	task_model_free(&p1);
	task_model_free(&p2);
	task_model_free(&s1);
	task_model_free(&s2);
}

void test_task_find_visible_index_locates_task_and_subtask(void) {
	task_t p1, p2, s1;
	task_create(project_id, 0, "P1", PRIORITY_P2, &p1);
	task_create(project_id, 0, "P2", PRIORITY_P1, &p2);
	task_create(project_id, p1.id, "S1", PRIORITY_P3, &s1);

	/* P2 (P1 priority) sorts first, then P1, then P1's own subtask. */
	TEST_ASSERT_EQUAL_INT(0, task_find_visible_index(project_id, false, p2.id));
	TEST_ASSERT_EQUAL_INT(1, task_find_visible_index(project_id, false, p1.id));
	TEST_ASSERT_EQUAL_INT(2, task_find_visible_index(project_id, false, s1.id));

	task_model_free(&p1);
	task_model_free(&p2);
	task_model_free(&s1);
}

void test_task_find_visible_index_returns_negative_one_when_not_found(void) {
	TEST_ASSERT_EQUAL_INT(-1, task_find_visible_index(project_id, false, 999999));
}

static int64_t insert_other_project(bool archived) {
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "panzerpi");
	int64_t id;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));
	if (archived) {
		p.id = id;
		p.archived = true;
		storage_project_update(&p);
	}
	return id;
}

void test_task_move_to_project_moves_top_level_task(void) {
	int64_t dest = insert_other_project(false);
	task_t t, moved;
	task_create(project_id, 0, "Move me", PRIORITY_P1, &t);
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_move_to_project(t.id, dest));
	storage_task_get(t.id, &moved);
	TEST_ASSERT_EQUAL_INT64(dest, moved.project_id);
	task_model_free(&t);
	task_model_free(&moved);
}

void test_task_move_to_project_rejects_invalid_moves(void) {
	int64_t dest = insert_other_project(false);
	int64_t archived_dest = insert_other_project(true);
	task_t parent, sub, archived_task;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Sub", PRIORITY_P3, &sub);
	task_create(project_id, 0, "Old", PRIORITY_P3, &archived_task);
	storage_task_set_completed(archived_task.id, true, false);
	int count = 0;
	task_archive_completed(project_id, &count, true);

	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_move_to_project(sub.id, dest));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_move_to_project(archived_task.id, dest));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_move_to_project(parent.id, project_id));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_move_to_project(parent.id, archived_dest));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_move_to_project(parent.id, 99999));

	task_t check;
	storage_task_get(parent.id, &check);
	TEST_ASSERT_EQUAL_INT64(project_id, check.project_id);
	task_model_free(&check);
	task_model_free(&parent);
	task_model_free(&sub);
	task_model_free(&archived_task);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_task_create_success);
	RUN_TEST(test_task_create_rejects_empty_title);
	RUN_TEST(test_task_create_rejects_invalid_priority);
	RUN_TEST(test_task_create_subtask_defaults_p3);
	RUN_TEST(test_task_create_rejects_nesting_a_subtask_under_a_subtask);
	RUN_TEST(test_task_update_fields_title_only_leaves_notes);
	RUN_TEST(test_task_set_priority_moves_to_end_of_new_group);
	RUN_TEST(test_task_set_completed_without_subtasks_toggles_immediately);
	RUN_TEST(test_task_set_completed_with_subtasks_requires_confirmation);
	RUN_TEST(test_task_set_completed_skips_confirmation_when_subtasks_already_match);
	RUN_TEST(test_task_delete_cascades_to_subtasks);
	RUN_TEST(test_task_delete_subtask_leaves_parent_and_siblings);
	RUN_TEST(test_task_clear_notes_leaves_task_intact);
	RUN_TEST(test_task_reorder_step_moves_within_group_and_stops_at_boundary);
	RUN_TEST(test_task_archive_completed_counts_and_applies_to_current_project_only);
	RUN_TEST(test_task_archive_completed_noop_when_none_eligible);
	RUN_TEST(test_task_restore_clears_only_archived_flag);
	RUN_TEST(test_task_list_visible_rows_interleaves_subtasks_under_their_parent);
	RUN_TEST(test_task_find_visible_index_locates_task_and_subtask);
	RUN_TEST(test_task_find_visible_index_returns_negative_one_when_not_found);
	RUN_TEST(test_task_move_to_project_moves_top_level_task);
	RUN_TEST(test_task_move_to_project_rejects_invalid_moves);
	RUN_TEST(test_task_set_completed_rejects_archived_task);
	RUN_TEST(test_task_set_completed_cascade_skips_archived_subtasks);
	RUN_TEST(test_task_archive_completed_archives_open_subtasks_with_parent);
	RUN_TEST(test_task_restore_keeps_parent_and_subtasks_together);
	return UNITY_END();
}
