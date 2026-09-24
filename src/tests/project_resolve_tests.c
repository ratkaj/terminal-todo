// lspdiag

#include <string.h>

#include <common.h>
#include <logger.h>
#include <project_resolve.h>
#include <storage.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "todo_project_resolve.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));
}

void tearDown(void) {
	storage_close();
	logger_close();
}

static int64_t insert_project(const char *name, const char *path) {
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "%s", name);
	p.canonical_path = (char *)path;
	int64_t id = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));
	return id;
}

void test_project_resolve_cwd_home_returns_today(void) {
	resolve_kind_t kind;
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user", "/home/user", &kind, &out, NULL, 0));
	TEST_ASSERT_EQUAL_INT(RESOLVE_HOME_TODAY, kind);
	TEST_ASSERT_EQUAL_STRING("Today", out.display_name);
	TEST_ASSERT_TRUE(out.builtin);
	project_model_free(&out);
}

void test_project_resolve_cwd_exact_registered_match(void) {
	int64_t id = insert_project("atomrpc", "/home/user/work/atomrpc");

	resolve_kind_t kind;
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user/work/atomrpc", "/home/user", &kind, &out, NULL, 0));
	TEST_ASSERT_EQUAL_INT(RESOLVE_REGISTERED, kind);
	TEST_ASSERT_EQUAL_INT64(id, out.id);
	project_model_free(&out);
}

void test_project_resolve_cwd_subdirectory_resolves_to_root(void) {
	int64_t id = insert_project("atomrpc", "/home/user/work/atomrpc");

	resolve_kind_t kind;
	project_t out;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user/work/atomrpc/src/plugins", "/home/user",
			&kind, &out, NULL, 0));
	TEST_ASSERT_EQUAL_INT(RESOLVE_REGISTERED, kind);
	TEST_ASSERT_EQUAL_INT64(id, out.id);
	project_model_free(&out);
}

void test_project_resolve_cwd_unregistered_directory_is_provisional(void) {
	resolve_kind_t kind;
	char provisional[256];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user/randomdir", "/home/user",
			&kind, NULL, provisional, sizeof(provisional)));
	TEST_ASSERT_EQUAL_INT(RESOLVE_PROVISIONAL, kind);
	TEST_ASSERT_EQUAL_STRING("/home/user/randomdir", provisional);
}

void test_project_resolve_cwd_home_subdirectory_not_special_cased(void) {
	resolve_kind_t kind;
	char provisional[256];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user/somedir", "/home/user",
			&kind, NULL, provisional, sizeof(provisional)));
	TEST_ASSERT_EQUAL_INT(RESOLVE_PROVISIONAL, kind);
}

void test_project_resolve_cwd_does_not_match_sibling_prefix(void) {
	insert_project("atomrpc", "/home/user/work/atomrpc");

	resolve_kind_t kind;
	char provisional[256];
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS,
		project_resolve_cwd("/home/user/work/atomrpc-other", "/home/user",
			&kind, NULL, provisional, sizeof(provisional)));
	TEST_ASSERT_EQUAL_INT(RESOLVE_PROVISIONAL, kind);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_project_resolve_cwd_home_returns_today);
	RUN_TEST(test_project_resolve_cwd_exact_registered_match);
	RUN_TEST(test_project_resolve_cwd_subdirectory_resolves_to_root);
	RUN_TEST(test_project_resolve_cwd_unregistered_directory_is_provisional);
	RUN_TEST(test_project_resolve_cwd_home_subdirectory_not_special_cased);
	RUN_TEST(test_project_resolve_cwd_does_not_match_sibling_prefix);
	return UNITY_END();
}
