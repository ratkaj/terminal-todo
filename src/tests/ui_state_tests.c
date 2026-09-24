// lspdiag

#include <string.h>

#include <logger.h>
#include <ui_state.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_ui_state.log");
}

void tearDown(void) {
	logger_close();
}

void test_app_state_init_defaults(void) {
	app_state_t st;
	app_state_init(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_EQUAL_INT(FOCUS_TASKS, st.focus);
	TEST_ASSERT_FALSE(st.archived_shown_projects);
	TEST_ASSERT_FALSE(st.archived_shown_tasks);
}

void test_app_state_focus_navigation_is_bounded(void) {
	app_state_t st;
	app_state_init(&st);

	app_state_focus_left(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_PROJECTS, st.focus);
	app_state_focus_left(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_PROJECTS, st.focus); /* no wrap past Projects */

	app_state_focus_right(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_TASKS, st.focus);
	app_state_focus_right(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_NOTES, st.focus);
	app_state_focus_right(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_NOTES, st.focus); /* no wrap past Notes */
}

void test_app_state_focus_navigation_disabled_outside_navigate_mode(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_task_form_new(&st, 1);

	app_state_focus_left(&st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_TASKS, st.focus); /* unchanged: form owns input */
}

void test_app_state_enter_task_form_new_defaults_p3(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_task_form_new(&st, 42);

	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_TRUE(st.task_form.is_new);
	TEST_ASSERT_FALSE(st.task_form.is_subtask);
	TEST_ASSERT_EQUAL_INT64(42, st.task_form.project_id);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P3, st.task_form.priority);
	TEST_ASSERT_EQUAL_STRING("", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(TASK_FORM_FIELD_NAME, st.task_form.field);
}

void test_app_state_enter_task_form_new_flags_provisional_context(void) {
	app_state_t st;
	app_state_init(&st);
	st.provisional_active = true;

	app_state_enter_task_form_new(&st, 0);

	TEST_ASSERT_TRUE(st.task_form.is_provisional);
}

void test_app_state_enter_task_form_new_not_provisional_by_default(void) {
	app_state_t st;
	app_state_init(&st);

	app_state_enter_task_form_new(&st, 42);

	TEST_ASSERT_FALSE(st.task_form.is_provisional);
}

void test_app_state_enter_task_form_new_not_provisional_when_viewing_real_project(void) {
	app_state_t st;
	app_state_init(&st);
	st.provisional_active = true; /* provisional project still exists in the list... */

	app_state_enter_task_form_new(&st, 42); /* ...but a different, real project is open */

	TEST_ASSERT_FALSE(st.task_form.is_provisional);
}

void test_app_state_enter_task_form_new_subtask_carries_parent_context(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_task_form_new_subtask(&st, 42, 7, "Implement project discovery");

	TEST_ASSERT_TRUE(st.task_form.is_subtask);
	TEST_ASSERT_EQUAL_INT64(7, st.task_form.parent_id);
	TEST_ASSERT_EQUAL_STRING("Implement project discovery", st.task_form.parent_title);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P3, st.task_form.priority);
}

void test_app_state_enter_task_form_edit_prefills_saved_values(void) {
	app_state_t st;
	app_state_init(&st);

	task_t t = {0};
	t.id = 5;
	t.project_id = 42;
	t.parent_id = 0;
	snprintf(t.title, sizeof(t.title), "Existing task");
	t.priority = PRIORITY_P1;

	app_state_enter_task_form_edit(&st, &t, NULL);

	TEST_ASSERT_FALSE(st.task_form.is_new);
	TEST_ASSERT_EQUAL_STRING("Existing task", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P1, st.task_form.priority);
	TEST_ASSERT_EQUAL_INT((int)strlen("Existing task"), (int)st.task_form.cursor);
}

void test_app_state_enter_project_form_new_prefills_and_places_cursor_at_end(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_project_form_new(&st, "atomrpc");

	TEST_ASSERT_EQUAL_INT(MODE_PROJECT_FORM, st.mode);
	TEST_ASSERT_FALSE(st.project_form.is_rename);
	TEST_ASSERT_EQUAL_STRING("atomrpc", st.project_form.name);
	TEST_ASSERT_EQUAL_INT(7, (int)st.project_form.cursor);
}

void test_app_state_enter_project_form_rename_carries_id(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_project_form_rename(&st, 9, "atomrpc");

	TEST_ASSERT_TRUE(st.project_form.is_rename);
	TEST_ASSERT_EQUAL_INT64(9, st.project_form.project_id);
	TEST_ASSERT_EQUAL_STRING("atomrpc", st.project_form.name);
}

void test_app_state_exit_form_returns_to_navigate(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_task_form_new(&st, 1);
	app_state_exit_form(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_app_state_confirm_lowercase_y_proceeds_without_suppressing(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_confirm(&st, CONFIRM_CAT_TASKS, "Delete task \"X\"?", 99, 7);
	TEST_ASSERT_EQUAL_INT(MODE_CONFIRM, st.mode);

	bool proceed = false;
	int action = -1;
	int64_t target_id = -1;
	app_state_confirm_answer(&st, 'y', &proceed, &action, &target_id);

	TEST_ASSERT_TRUE(proceed);
	TEST_ASSERT_EQUAL_INT(99, action);
	TEST_ASSERT_EQUAL_INT64(7, target_id);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&st.confirm, CONFIRM_CAT_TASKS));
}

void test_app_state_confirm_uppercase_Y_suppresses_future_prompts(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_confirm(&st, CONFIRM_CAT_TASKS, "Delete task \"X\"?", 1, 3);

	bool proceed = false;
	app_state_confirm_answer(&st, 'Y', &proceed, NULL, NULL);

	TEST_ASSERT_TRUE(proceed);
	TEST_ASSERT_FALSE(confirm_state_should_prompt(&st.confirm, CONFIRM_CAT_TASKS));
}

void test_app_state_reorder_enter_and_exit(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_enter_reorder(&st, 5, 0);
	TEST_ASSERT_EQUAL_INT(MODE_REORDER, st.mode);
	TEST_ASSERT_EQUAL_INT64(5, st.reorder.task_id);

	app_state_exit_reorder(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_app_state_toggle_help(void) {
	app_state_t st;
	app_state_init(&st);
	app_state_toggle_help(&st);
	TEST_ASSERT_EQUAL_INT(MODE_HELP, st.mode);
	app_state_toggle_help(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_app_state_project_switcher_resets_query(void) {
	app_state_t st;
	app_state_init(&st);
	snprintf(st.switcher_query, sizeof(st.switcher_query), "stale");
	st.switcher_sel = 3;
	app_state_enter_project_switcher(&st);
	TEST_ASSERT_EQUAL_INT(MODE_PROJECT_SWITCHER, st.mode);
	TEST_ASSERT_EQUAL_STRING("", st.switcher_query);
	TEST_ASSERT_EQUAL_INT(0, st.switcher_sel);

	app_state_exit_project_switcher(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_app_state_task_move_enter_and_exit(void) {
	app_state_t st;
	app_state_init(&st);
	st.task_move.sel = 5;
	app_state_enter_task_move(&st, 42, "Write docs");
	TEST_ASSERT_EQUAL_INT(MODE_TASK_MOVE, st.mode);
	TEST_ASSERT_EQUAL_INT64(42, st.task_move.task_id);
	TEST_ASSERT_EQUAL_STRING("Write docs", st.task_move.title);
	TEST_ASSERT_EQUAL_INT(0, st.task_move.sel);
	app_state_exit_task_move(&st);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_app_state_init_defaults);
	RUN_TEST(test_app_state_focus_navigation_is_bounded);
	RUN_TEST(test_app_state_focus_navigation_disabled_outside_navigate_mode);
	RUN_TEST(test_app_state_enter_task_form_new_defaults_p3);
	RUN_TEST(test_app_state_enter_task_form_new_flags_provisional_context);
	RUN_TEST(test_app_state_enter_task_form_new_not_provisional_by_default);
	RUN_TEST(test_app_state_enter_task_form_new_not_provisional_when_viewing_real_project);
	RUN_TEST(test_app_state_enter_task_form_new_subtask_carries_parent_context);
	RUN_TEST(test_app_state_enter_task_form_edit_prefills_saved_values);
	RUN_TEST(test_app_state_enter_project_form_new_prefills_and_places_cursor_at_end);
	RUN_TEST(test_app_state_enter_project_form_rename_carries_id);
	RUN_TEST(test_app_state_exit_form_returns_to_navigate);
	RUN_TEST(test_app_state_confirm_lowercase_y_proceeds_without_suppressing);
	RUN_TEST(test_app_state_confirm_uppercase_Y_suppresses_future_prompts);
	RUN_TEST(test_app_state_reorder_enter_and_exit);
	RUN_TEST(test_app_state_toggle_help);
	RUN_TEST(test_app_state_project_switcher_resets_query);
	RUN_TEST(test_app_state_task_move_enter_and_exit);
	return UNITY_END();
}
