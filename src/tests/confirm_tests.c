// lspdiag

#include <confirm.h>
#include <logger.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_confirm.log");
}

void tearDown(void) {
	logger_close();
}

void test_confirm_state_init_all_should_prompt(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_PROJECTS));
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_TASKS));
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_NOTES));
}

void test_confirm_state_lowercase_y_proceeds_without_suppressing(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	bool proceed = false;
	confirm_state_apply_answer(&cs, CONFIRM_CAT_TASKS, 'y', &proceed);
	TEST_ASSERT_TRUE(proceed);
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_TASKS));
}

void test_confirm_state_uppercase_Y_proceeds_and_suppresses(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	bool proceed = false;
	confirm_state_apply_answer(&cs, CONFIRM_CAT_TASKS, 'Y', &proceed);
	TEST_ASSERT_TRUE(proceed);
	TEST_ASSERT_FALSE(confirm_state_should_prompt(&cs, CONFIRM_CAT_TASKS));
}

void test_confirm_state_n_cancels(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	bool proceed = true;
	confirm_state_apply_answer(&cs, CONFIRM_CAT_TASKS, 'n', &proceed);
	TEST_ASSERT_FALSE(proceed);
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_TASKS));
}

void test_confirm_state_other_key_cancels(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	bool proceed = true;
	confirm_state_apply_answer(&cs, CONFIRM_CAT_TASKS, 'x', &proceed);
	TEST_ASSERT_FALSE(proceed);
}

void test_confirm_state_categories_are_independent(void) {
	confirm_state_t cs;
	confirm_state_init(&cs);
	bool proceed = false;
	confirm_state_apply_answer(&cs, CONFIRM_CAT_PROJECTS, 'Y', &proceed);
	TEST_ASSERT_FALSE(confirm_state_should_prompt(&cs, CONFIRM_CAT_PROJECTS));
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_TASKS));
	TEST_ASSERT_TRUE(confirm_state_should_prompt(&cs, CONFIRM_CAT_NOTES));
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_confirm_state_init_all_should_prompt);
	RUN_TEST(test_confirm_state_lowercase_y_proceeds_without_suppressing);
	RUN_TEST(test_confirm_state_uppercase_Y_proceeds_and_suppresses);
	RUN_TEST(test_confirm_state_n_cancels);
	RUN_TEST(test_confirm_state_other_key_cancels);
	RUN_TEST(test_confirm_state_categories_are_independent);
	return UNITY_END();
}
