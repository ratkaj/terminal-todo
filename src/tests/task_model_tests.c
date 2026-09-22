// lspdiag

#include <string.h>

#include <common.h>
#include <logger.h>
#include <task_model.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_task_model.log");
}

void tearDown(void) {
	logger_close();
}

void test_task_model_validate_priority_accepts_valid_values(void) {
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_model_validate_priority(PRIORITY_P1));
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_model_validate_priority(PRIORITY_P2));
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, task_model_validate_priority(PRIORITY_P3));
}

void test_task_model_validate_priority_rejects_invalid_values(void) {
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_model_validate_priority(0));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_model_validate_priority(4));
	TEST_ASSERT_EQUAL_INT(RT_ERROR, task_model_validate_priority(-1));
}

void test_task_model_free_handles_null(void) {
	task_model_free(NULL);
}

void test_task_model_free_frees_notes(void) {
	task_t t = {0};
	t.notes = strdup("hello");
	TEST_ASSERT_NOT_NULL(t.notes);
	task_model_free(&t);
	TEST_ASSERT_NULL(t.notes);
}

void test_project_model_free_handles_null(void) {
	project_model_free(NULL);
}

void test_project_model_free_frees_path(void) {
	project_t p = {0};
	p.canonical_path = strdup("/home/user");
	TEST_ASSERT_NOT_NULL(p.canonical_path);
	project_model_free(&p);
	TEST_ASSERT_NULL(p.canonical_path);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_task_model_validate_priority_accepts_valid_values);
	RUN_TEST(test_task_model_validate_priority_rejects_invalid_values);
	RUN_TEST(test_task_model_free_handles_null);
	RUN_TEST(test_task_model_free_frees_notes);
	RUN_TEST(test_project_model_free_handles_null);
	RUN_TEST(test_project_model_free_frees_path);
	return UNITY_END();
}
