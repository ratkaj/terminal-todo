// lspdiag

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <input_dispatch.h>
#include <report.h>
#include <logger.h>
#include <project.h>
#include <storage.h>
#include <task.h>
#include <unity/unity.h>

static int64_t project_id;
static app_state_t st;

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_input_dispatch.log");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_open(":memory:"));

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "atomrpc");
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &project_id));

	app_state_init(&st);
	st.current_project_id = project_id;
	st.focus = FOCUS_TASKS;
}

void tearDown(void) {
	storage_close();
	logger_close();
}

static void type_text(const char *text) {
	for (const char *c = text; *c; c++)
		input_dispatch_key((unsigned char)*c, &st, LAYOUT_WIDE);
}

void test_task_form_text_entry_does_not_trigger_navigation_shortcuts(void) {
	app_state_enter_task_form_new(&st, project_id);
	type_text("i1n");
	TEST_ASSERT_EQUAL_STRING("i1n", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode); /* 'n'/'i' didn't leave the form */
	TEST_ASSERT_EQUAL_INT(PRIORITY_P3, st.task_form.priority); /* '1' didn't select priority */
}

void test_task_form_accepts_croatian_letters_typed_byte_by_byte(void) {
	app_state_enter_task_form_new(&st, project_id);
	type_text("Čišćenje đaka");
	TEST_ASSERT_EQUAL_STRING("Čišćenje đaka", st.task_form.name);
	TEST_ASSERT_EQUAL_size_t(strlen("Čišćenje đaka"), st.task_form.cursor);
}

void test_task_form_cursor_and_backspace_move_by_character(void) {
	app_state_enter_task_form_new(&st, project_id);
	type_text("ač日😀");
	input_dispatch_key(KEY_LEFT, &st, LAYOUT_WIDE);  /* before 😀 */
	input_dispatch_key(KEY_LEFT, &st, LAYOUT_WIDE);  /* before 日 */
	TEST_ASSERT_EQUAL_size_t(strlen("ač"), st.task_form.cursor);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE); /* deletes č */
	TEST_ASSERT_EQUAL_STRING("a日😀", st.task_form.name);
	input_dispatch_key(KEY_RIGHT, &st, LAYOUT_WIDE); /* after 日 */
	type_text("ž");
	TEST_ASSERT_EQUAL_STRING("a日ž😀", st.task_form.name);
}

void test_task_form_drops_partial_character_interrupted_by_another_key(void) {
	app_state_enter_task_form_new(&st, project_id);
	input_dispatch_key(0xC4, &st, LAYOUT_WIDE); /* first byte of č */
	input_dispatch_key('x', &st, LAYOUT_WIDE);
	input_dispatch_key(0x8D, &st, LAYOUT_WIDE); /* orphaned second byte */
	TEST_ASSERT_EQUAL_STRING("x", st.task_form.name);
}

void test_task_form_full_name_never_ends_in_half_a_character(void) {
	app_state_enter_task_form_new(&st, project_id);
	/* Fill to one byte short of the limit, then try a 2-byte letter. */
	char fill[TASK_TITLE_MAX];
	memset(fill, 'a', sizeof(fill) - 2);
	fill[sizeof(fill) - 2] = '\0';
	type_text(fill);
	type_text("č");
	TEST_ASSERT_EQUAL_STRING(fill, st.task_form.name);
}

void test_project_form_accepts_non_ascii_and_backspaces_whole_character(void) {
	app_state_enter_project_form_new(&st, "");
	type_text("Šuma");
	TEST_ASSERT_EQUAL_STRING("Šuma", st.project_form.name);
	for (int i = 0; i < 3; i++)
		input_dispatch_key(KEY_LEFT, &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_STRING("uma", st.project_form.name);
}

void test_project_switcher_query_accepts_non_ascii(void) {
	app_state_enter_project_switcher(&st);
	type_text("čvor");
	TEST_ASSERT_EQUAL_STRING("čvor", st.switcher_query);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_STRING("č", st.switcher_query);
	input_dispatch_key(KEY_BACKSPACE, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_STRING("", st.switcher_query);
}

void test_task_form_tab_cycles_and_wraps(void) {
	app_state_enter_task_form_new(&st, project_id);
	TEST_ASSERT_EQUAL_INT(TASK_FORM_FIELD_NAME, st.task_form.field);
	input_dispatch_key(9, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(TASK_FORM_FIELD_PRIORITY, st.task_form.field);
	input_dispatch_key(9, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(TASK_FORM_FIELD_NAME, st.task_form.field);
}

void test_task_form_digit_selects_priority_only_when_priority_focused(void) {
	app_state_enter_task_form_new(&st, project_id);
	input_dispatch_key('2', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_STRING("2", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P3, st.task_form.priority);

	input_dispatch_key(9, &st, LAYOUT_WIDE); /* Tab to Priority */
	input_dispatch_key('1', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P1, st.task_form.priority);
	TEST_ASSERT_EQUAL_STRING("2", st.task_form.name); /* Name untouched */
}

void test_task_form_enter_submits_from_either_field(void) {
	app_state_enter_task_form_new(&st, project_id);
	type_text("New task");
	input_dispatch_key(9, &st, LAYOUT_WIDE); /* focus Priority */
	input_dispatch_key('1', &st, LAYOUT_WIDE);
	dispatch_result_t r = input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, r);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(1, (int)n);
	TEST_ASSERT_EQUAL_STRING("New task", arr[0].title);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P1, arr[0].priority);
	storage_task_array_free(arr, n);
}

void test_task_form_esc_cancels_without_saving(void) {
	app_state_enter_task_form_new(&st, project_id);
	type_text("Discard me");
	input_dispatch_key(27, &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(0, (int)n);
	storage_task_array_free(arr, n);
}

void test_navigate_tasks_enter_opens_edit_form_with_saved_values(void) {
	task_t t;
	task_create(project_id, 0, "Existing", PRIORITY_P2, &t);
	st.task_sel = 0;

	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_FALSE(st.task_form.is_new);
	TEST_ASSERT_EQUAL_STRING("Existing", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P2, st.task_form.priority);

	task_model_free(&t);
}

void test_navigate_tasks_r_key_opens_edit_form_same_as_enter(void) {
	task_t t;
	task_create(project_id, 0, "Existing", PRIORITY_P2, &t);
	st.task_sel = 0;

	input_dispatch_key('r', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_FALSE(st.task_form.is_new);
	TEST_ASSERT_EQUAL_STRING("Existing", st.task_form.name);
	TEST_ASSERT_EQUAL_INT(PRIORITY_P2, st.task_form.priority);

	task_model_free(&t);
}

void test_navigate_subtask_creation_on_top_level_selection(void) {
	task_t parent;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);

	st.task_sel = 0; /* the top-level task row */
	dispatch_result_t r = input_dispatch_key('s', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, r);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_TRUE(st.task_form.is_subtask);
	TEST_ASSERT_EQUAL_INT64(parent.id, st.task_form.parent_id);
	TEST_ASSERT_EQUAL_STRING("Parent", st.task_form.parent_title);

	task_model_free(&parent);
}

void test_navigate_subtask_creation_on_subtask_selection_chains_under_same_parent(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub);

	st.task_sel = 1; /* the subtask row, per visible-rows interleaving */
	dispatch_result_t r = input_dispatch_key('s', &st, LAYOUT_WIDE);

	/* Pressing 's' again while a subtask (not its parent) is selected must
	   still create a sibling under the same parent, not be a no-op - this is
	   what lets a user add several subtasks in a row without re-selecting
	   the top-level task each time. */
	TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, r);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_TRUE(st.task_form.is_subtask);
	TEST_ASSERT_EQUAL_INT64(parent.id, st.task_form.parent_id);
	TEST_ASSERT_EQUAL_STRING("Parent", st.task_form.parent_title);

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_navigate_reorder_mode_moves_and_finishes(void) {
	task_t a, b;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);

	st.task_sel = 0; /* A */
	input_dispatch_key('o', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_REORDER, st.mode);
	TEST_ASSERT_EQUAL_INT64(a.id, st.reorder.task_id);

	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE); /* A moves below B */
	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_STRING("B", arr[0].title);
	TEST_ASSERT_EQUAL_STRING("A", arr[1].title);
	storage_task_array_free(arr, n);

	/* A is now at the bottom of its group; moving down again is a no-op boundary. */
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);

	input_dispatch_key('\n', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	task_model_free(&a);
	task_model_free(&b);
}

void test_navigate_delete_confirms_then_suppresses_within_category(void) {
	task_t a, b;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);

	st.task_sel = 0;
	input_dispatch_key('d', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_CONFIRM, st.mode);

	input_dispatch_key('Y', &st, LAYOUT_WIDE); /* delete + suppress future task-category confirms */
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(1, (int)n);
	TEST_ASSERT_EQUAL_STRING("B", arr[0].title);
	storage_task_array_free(arr, n);

	/* Suppressed: deleting again applies immediately without a confirm prompt. */
	st.task_sel = 0;
	input_dispatch_key('d', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(0, (int)n);
	storage_task_array_free(arr, n);

	task_model_free(&a);
	task_model_free(&b);
}

void test_navigate_space_completion_requires_confirmation_for_subtasks(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Child", PRIORITY_P3, &sub);

	st.task_sel = 0; /* parent */
	input_dispatch_key(' ', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_CONFIRM, st.mode);

	input_dispatch_key('y', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	task_t fetched;
	storage_task_get(parent.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);
	storage_task_get(sub.id, &fetched);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	task_model_free(&parent);
	task_model_free(&sub);
}

void test_navigate_archive_completed_and_restore_contextual_a(void) {
	task_t a;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	st.task_sel = 0;
	input_dispatch_key(' ', &st, LAYOUT_WIDE); /* complete, no subtasks: immediate */
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	input_dispatch_key('a', &st, LAYOUT_WIDE); /* archived hidden: archive completed */
	TEST_ASSERT_EQUAL_INT(MODE_CONFIRM, st.mode);
	input_dispatch_key('y', &st, LAYOUT_WIDE);

	task_t fetched;
	storage_task_get(a.id, &fetched);
	TEST_ASSERT_TRUE(fetched.archived);
	task_model_free(&fetched);

	input_dispatch_key('A', &st, LAYOUT_WIDE); /* display archived */
	TEST_ASSERT_TRUE(st.archived_shown_tasks);

	st.task_sel = 0;
	input_dispatch_key('a', &st, LAYOUT_WIDE); /* restore selected archived task */
	storage_task_get(a.id, &fetched);
	TEST_ASSERT_FALSE(fetched.archived);
	TEST_ASSERT_EQUAL_INT(TASK_STATUS_COMPLETED, fetched.status);
	task_model_free(&fetched);

	task_model_free(&a);
}

void test_pane_navigation_bounded_and_disabled_during_form(void) {
	input_dispatch_key(KEY_LEFT, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_PROJECTS, st.focus);
	input_dispatch_key(KEY_LEFT, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(FOCUS_PROJECTS, st.focus);

	app_state_enter_task_form_new(&st, project_id);
	input_dispatch_key(KEY_RIGHT, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode); /* form still owns input */
}

void test_help_toggle(void) {
	TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, input_dispatch_key('?', &st, LAYOUT_WIDE));
	TEST_ASSERT_EQUAL_INT(MODE_HELP, st.mode);
	input_dispatch_key('?', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_quit_returns_quit_action(void) {
	TEST_ASSERT_EQUAL_INT(ACTION_QUIT, input_dispatch_key('q', &st, LAYOUT_WIDE));
}

void test_project_switcher_filters_and_selects(void) {
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "panzerpi");
	int64_t other_id;
	storage_project_insert(&other, &other_id);

	input_dispatch_key('p', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_PROJECT_SWITCHER, st.mode);

	type_text("atom");
	TEST_ASSERT_EQUAL_STRING("atom", st.switcher_query);

	input_dispatch_key('\n', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_EQUAL_INT(FOCUS_TASKS, st.focus);
	TEST_ASSERT_EQUAL_INT64(project_id, st.current_project_id);
}

void test_provisional_project_committed_atomically_on_first_task(void) {
	app_state_init(&st);
	st.focus = FOCUS_TASKS;
	st.provisional_active = true;
	snprintf(st.provisional_project.display_name,
		sizeof(st.provisional_project.display_name), "atomrpc-new");
	st.provisional_project.canonical_path = strdup("/home/user/work/atomrpc-new");
	st.provisional_project.id = 0;
	st.current_project_id = 0;

	/* 'i' must work even though current_project_id is 0: a provisional
	   project is an empty task list to view/add to, not "no project". */
	dispatch_result_t r = input_dispatch_key('i', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, r);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	TEST_ASSERT_TRUE(st.task_form.is_provisional);

	type_text("First task");
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_FALSE(st.provisional_active);
	TEST_ASSERT_NOT_EQUAL(0, st.current_project_id);

	project_t saved;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_get(st.current_project_id, &saved));
	TEST_ASSERT_EQUAL_STRING("atomrpc-new", saved.display_name);
	TEST_ASSERT_EQUAL_STRING("/home/user/work/atomrpc-new", saved.canonical_path);
	project_model_free(&saved);

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(st.current_project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(1, (int)n);
	TEST_ASSERT_EQUAL_STRING("First task", arr[0].title);
	storage_task_array_free(arr, n);
}

void test_navigate_projects_arrow_updates_current_project_live(void) {
	int64_t other_id;
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "zzz-other");
	storage_project_insert(&other, &other_id);

	st.focus = FOCUS_PROJECTS;
	st.project_sel = 0;

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(false, &arr, &n);
	TEST_ASSERT_TRUE(n >= 2);

	/* Up/Down alone (no Enter) must already update current_project_id, so
	   the Tasks pane can preview the highlighted project live. */
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT64(arr[1].id, st.current_project_id);
	TEST_ASSERT_EQUAL_INT(FOCUS_PROJECTS, st.focus);

	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT64(arr[0].id, st.current_project_id);

	storage_project_array_free(arr, n);
}

static void assert_delete_highlighted_project_moves_to_next(bool suppress) {
	int64_t a_id, b_id;
	project_t a = {0}, b = {0};
	snprintf(a.display_name, sizeof(a.display_name), "aaa-delete-me");
	snprintf(b.display_name, sizeof(b.display_name), "aab-next");
	storage_project_insert(&a, &a_id);
	storage_project_insert(&b, &b_id);

	st.focus = FOCUS_PROJECTS;
	st.project_sel = project_find_index(false, a_id);
	st.current_project_id = a_id;
	if (suppress)
		st.confirm.suppressed[CONFIRM_CAT_PROJECTS] = true;

	input_dispatch_key('d', &st, LAYOUT_WIDE);
	if (!suppress)
		input_dispatch_key('y', &st, LAYOUT_WIDE);

	/* The row below slides up under the highlight and Tasks shows it, so
	   'i' still has a live project to add to. */
	TEST_ASSERT_EQUAL_INT(-1, project_find_index(true, a_id));
	TEST_ASSERT_EQUAL_INT64(b_id, st.current_project_id);
	TEST_ASSERT_EQUAL_INT(project_find_index(false, b_id), st.project_sel);

	st.focus = FOCUS_TASKS;
	TEST_ASSERT_NOT_EQUAL(ACTION_NONE, input_dispatch_key('i', &st, LAYOUT_WIDE));
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
}

void test_navigate_projects_delete_current_after_confirm_selects_next(void) {
	assert_delete_highlighted_project_moves_to_next(false);
}

void test_navigate_projects_delete_current_without_prompt_selects_next(void) {
	assert_delete_highlighted_project_moves_to_next(true);
}

static int64_t add_project(const char *name) {
	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "%s", name);
	int64_t id = 0;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_insert(&p, &id));
	return id;
}

static void assert_highlight_is_current(void) {
	int idx = project_find_index(st.archived_shown_projects, st.current_project_id);
	TEST_ASSERT_TRUE(idx >= 0);
	TEST_ASSERT_EQUAL_INT(idx + (st.provisional_active ? 1 : 0), st.project_sel);
}

void test_new_project_form_enter_shows_new_project_in_tasks(void) {
	st.focus = FOCUS_PROJECTS;
	st.project_sel = project_find_index(false, project_id);
	app_state_enter_project_form_new(&st, "");
	type_text("zzz-fresh");
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_NOT_EQUAL(project_id, st.current_project_id);
	project_t p;
	TEST_ASSERT_EQUAL_INT(RT_SUCCESS, storage_project_get(st.current_project_id, &p));
	TEST_ASSERT_EQUAL_STRING("zzz-fresh", p.display_name);
	project_model_free(&p);
	assert_highlight_is_current();
}

void test_project_rename_highlight_follows_resorted_project(void) {
	int64_t alpha = add_project("alpha");
	add_project("beta");
	st.focus = FOCUS_PROJECTS;
	st.current_project_id = alpha;
	st.project_sel = project_find_index(false, alpha);

	input_dispatch_key('r', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_PROJECT_FORM, st.mode);
	snprintf(st.project_form.name, sizeof(st.project_form.name), "zulu");
	st.project_form.cursor = strlen(st.project_form.name);
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT64(alpha, st.current_project_id);
	assert_highlight_is_current();
}

void test_project_archive_current_shows_project_now_highlighted(void) {
	int64_t alpha = add_project("aaa-archive-me");
	int64_t beta = add_project("aab-next");
	st.focus = FOCUS_PROJECTS;
	st.current_project_id = alpha;
	st.project_sel = project_find_index(false, alpha);

	input_dispatch_key('a', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT64(beta, st.current_project_id);
	assert_highlight_is_current();
}

void test_project_switcher_highlight_accounts_for_provisional_row(void) {
	int64_t other = add_project("panzerpi");
	st.provisional_active = true;
	snprintf(st.provisional_project.display_name,
		sizeof(st.provisional_project.display_name), "newdir");

	input_dispatch_key('p', &st, LAYOUT_WIDE);
	type_text("panzer");
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT64(other, st.current_project_id);
	assert_highlight_is_current();
}

void test_task_delete_last_row_keeps_selection_on_new_last_row(void) {
	task_t a, b, c;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);
	task_create(project_id, 0, "C", PRIORITY_P3, &c);
	st.confirm.suppressed[CONFIRM_CAT_TASKS] = true;

	st.task_sel = 2;
	input_dispatch_key('d', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(1, st.task_sel);

	/* Up from B lands on A, not skipping a row. */
	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(0, st.task_sel);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_archive_completed_clamps_selection_after_confirm(void) {
	task_t a, b;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);
	int count = 0;
	task_set_completed(b.id, true, true, &count);

	st.task_sel = 1; /* completed B sorts last */
	input_dispatch_key('a', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_CONFIRM, st.mode);
	input_dispatch_key('y', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(0, st.task_sel);

	task_model_free(&a);
	task_model_free(&b);
}

void test_navigate_projects_can_move_off_provisional_project(void) {
	st.provisional_active = true;
	snprintf(st.provisional_project.display_name,
		sizeof(st.provisional_project.display_name), "newdir");
	st.provisional_project.id = 0;
	st.current_project_id = 0;
	st.project_sel = 0;
	st.focus = FOCUS_PROJECTS;

	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list(false, &arr, &n);
	TEST_ASSERT_TRUE(n >= 1);

	/* Previously stuck: current_project_id stayed 0 no matter how far Down
	   was pressed, because the provisional row was always drawn/treated as
	   selected while current_project_id == 0. */
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(1, st.project_sel);
	TEST_ASSERT_EQUAL_INT64(arr[0].id, st.current_project_id);

	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(0, st.project_sel);
	TEST_ASSERT_EQUAL_INT64(0, st.current_project_id);

	storage_project_array_free(arr, n);
}

void test_navigate_projects_n_key_is_not_new_project_shortcut_anymore(void) {
	st.focus = FOCUS_PROJECTS;
	dispatch_result_t r = input_dispatch_key('n', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(ACTION_NONE, r);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
}

void test_navigate_tasks_n_key_returns_edit_notes_action(void) {
	task_t t;
	task_create(project_id, 0, "A", PRIORITY_P3, &t);
	st.task_sel = 0;

	TEST_ASSERT_EQUAL_INT(ACTION_EDIT_NOTES, input_dispatch_key('n', &st, LAYOUT_WIDE));

	task_model_free(&t);
}

void test_navigate_notes_enter_and_c_key_trigger_actions(void) {
	task_t t;
	task_create(project_id, 0, "A", PRIORITY_P3, &t);
	st.task_sel = 0;
	st.focus = FOCUS_NOTES;

	TEST_ASSERT_EQUAL_INT(ACTION_EDIT_NOTES, input_dispatch_key('\n', &st, LAYOUT_WIDE));
	TEST_ASSERT_EQUAL_INT(ACTION_COPY_NOTES, input_dispatch_key('c', &st, LAYOUT_WIDE));

	task_model_free(&t);
}

void test_navigate_e_key_exports_only_from_tasks(void) {
	/* No task needs to be selected: export covers the whole project. */
	TEST_ASSERT_EQUAL_INT(ACTION_EXPORT, input_dispatch_key('e', &st, LAYOUT_WIDE));

	st.focus = FOCUS_NOTES;
	TEST_ASSERT_EQUAL_INT(ACTION_NONE, input_dispatch_key('e', &st, LAYOUT_WIDE));
	st.focus = FOCUS_PROJECTS;
	TEST_ASSERT_EQUAL_INT(ACTION_NONE, input_dispatch_key('e', &st, LAYOUT_WIDE));

	st.focus = FOCUS_TASKS;
	app_state_enter_task_form_new(&st, project_id);
	TEST_ASSERT_NOT_EQUAL(ACTION_EXPORT, input_dispatch_key('e', &st, LAYOUT_WIDE));
	TEST_ASSERT_EQUAL_STRING("e", st.task_form.name);
}

void test_task_form_new_task_focuses_created_task(void) {
	task_t existing;
	task_create(project_id, 0, "Existing P1", PRIORITY_P1, &existing);

	st.task_sel = 0;
	app_state_enter_task_form_new(&st, project_id);
	type_text("New P3 task");
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_INT(2, (int)n);
	/* "Existing" is P1 and sorts first; the new P3 task lands at index 1,
	   and task_sel must follow it there instead of staying at 0. */
	TEST_ASSERT_EQUAL_STRING("New P3 task", arr[1].title);
	TEST_ASSERT_EQUAL_INT(1, st.task_sel);
	storage_task_array_free(arr, n);

	task_model_free(&existing);
}

void test_reorder_task_sel_follows_moved_task(void) {
	task_t a, b;
	task_create(project_id, 0, "A", PRIORITY_P3, &a);
	task_create(project_id, 0, "B", PRIORITY_P3, &b);

	st.task_sel = 0; /* A */
	input_dispatch_key('o', &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE); /* A moves below B */

	TEST_ASSERT_EQUAL_INT(1, st.task_sel);

	task_model_free(&a);
	task_model_free(&b);
}

void test_task_form_priority_change_task_sel_follows_edited_task(void) {
	task_t a, b, c;
	task_create(project_id, 0, "A", PRIORITY_P1, &a);
	task_create(project_id, 0, "B", PRIORITY_P2, &b);
	task_create(project_id, 0, "C", PRIORITY_P3, &c);

	st.task_sel = 2;
	input_dispatch_key('\n', &st, LAYOUT_WIDE); /* edit C */
	TEST_ASSERT_EQUAL_INT(MODE_TASK_FORM, st.mode);
	input_dispatch_key(9, &st, LAYOUT_WIDE);     /* Tab to Priority */
	input_dispatch_key('1', &st, LAYOUT_WIDE);
	input_dispatch_key('\n', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	/* New order is A, C, B; the selection stays on C. */
	TEST_ASSERT_EQUAL_INT(1, st.task_sel);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

void test_priority_change_task_sel_follows_reordered_task(void) {
	task_t a, b, c;
	task_create(project_id, 0, "A", PRIORITY_P1, &a);
	task_create(project_id, 0, "B", PRIORITY_P2, &b);
	task_create(project_id, 0, "C", PRIORITY_P3, &c);

	st.task_sel = 2; /* C, last by priority group */
	input_dispatch_key('1', &st, LAYOUT_WIDE); /* C becomes P1, moves ahead of B */

	/* New order is A, C, B: A and C share the P1 group (C appended after
	   A), then B still in P2 - selection must follow C to index 1, not stay
	   on whatever task now occupies index 2 (B). */
	TEST_ASSERT_EQUAL_INT(1, st.task_sel);

	task_t *arr = NULL;
	size_t n = 0;
	task_list_visible_rows(project_id, false, &arr, &n);
	TEST_ASSERT_EQUAL_STRING("C", arr[st.task_sel].title);
	storage_task_array_free(arr, n);

	task_model_free(&a);
	task_model_free(&b);
	task_model_free(&c);
}

static int64_t move_target_index(int64_t dest) {
	project_t *arr = NULL;
	size_t n = 0;
	storage_project_list_move_targets(project_id, &arr, &n);
	int64_t idx = -1;
	for (size_t i = 0; i < n; i++)
		if (arr[i].id == dest)
			idx = (int64_t)i;
	storage_project_array_free(arr, n);
	return idx;
}

void test_move_task_esc_leaves_task_in_place(void) {
	task_t t;
	task_create(project_id, 0, "Stay", PRIORITY_P3, &t);
	input_dispatch_key('m', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_TASK_MOVE, st.mode);
	TEST_ASSERT_EQUAL_INT64(t.id, st.task_move.task_id);
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	input_dispatch_key(27, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);

	task_t check;
	storage_task_get(t.id, &check);
	TEST_ASSERT_EQUAL_INT64(project_id, check.project_id);
	task_model_free(&check);
	task_model_free(&t);
}

void test_move_task_enter_moves_and_clamps_selection(void) {
	project_t other = {0};
	snprintf(other.display_name, sizeof(other.display_name), "panzerpi");
	int64_t dest;
	storage_project_insert(&other, &dest);
	task_t first, last;
	task_create(project_id, 0, "First", PRIORITY_P3, &first);
	task_create(project_id, 0, "Last", PRIORITY_P3, &last);
	st.task_sel = 1;

	input_dispatch_key('m', &st, LAYOUT_WIDE);
	int64_t idx = move_target_index(dest);
	TEST_ASSERT_TRUE(idx >= 0);
	for (int64_t i = 0; i < idx; i++)
		input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	input_dispatch_key('\n', &st, LAYOUT_WIDE);

	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_EQUAL_INT64(project_id, st.current_project_id);
	TEST_ASSERT_EQUAL_INT(0, st.task_sel);
	task_t check;
	storage_task_get(last.id, &check);
	TEST_ASSERT_EQUAL_INT64(dest, check.project_id);
	task_model_free(&check);
	task_model_free(&first);
	task_model_free(&last);
}

void test_move_key_ignored_on_subtask(void) {
	task_t parent, sub;
	task_create(project_id, 0, "Parent", PRIORITY_P3, &parent);
	task_create(project_id, parent.id, "Sub", PRIORITY_P3, &sub);
	st.task_sel = 1;
	input_dispatch_key('m', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	task_model_free(&parent);
	task_model_free(&sub);
}

void test_help_scrolls_and_reopens_at_top(void) {
	input_dispatch_key('?', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(MODE_HELP, st.mode);
	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(0, st.help_scroll);
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(2, st.help_scroll);
	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(1, st.help_scroll);
	input_dispatch_key('?', &st, LAYOUT_WIDE);
	input_dispatch_key('?', &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(0, st.help_scroll);
}

void test_report_menu_opens_from_every_pane(void) {
	pane_focus_t panes[] = { FOCUS_PROJECTS, FOCUS_TASKS, FOCUS_NOTES };
	for (size_t i = 0; i < 3; i++) {
		st.focus = panes[i];
		TEST_ASSERT_EQUAL_INT(ACTION_REDRAW, input_dispatch_key('g', &st, LAYOUT_WIDE));
		TEST_ASSERT_EQUAL_INT(MODE_REPORT_MENU, st.mode);
		input_dispatch_key(27, &st, LAYOUT_WIDE);
		TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
		TEST_ASSERT_EQUAL_INT(panes[i], st.focus);
	}
}

void test_report_menu_selects_period_and_returns_report_action(void) {
	input_dispatch_key('g', &st, LAYOUT_WIDE);
	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(REPORT_THIS_WEEK, st.report_sel);
	for (int i = 0; i < 10; i++)
		input_dispatch_key(KEY_DOWN, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(REPORT_LAST_MONTH, st.report_sel);
	input_dispatch_key(KEY_UP, &st, LAYOUT_WIDE);
	TEST_ASSERT_EQUAL_INT(ACTION_REPORT, input_dispatch_key('\n', &st, LAYOUT_WIDE));
	TEST_ASSERT_EQUAL_INT(MODE_NAVIGATE, st.mode);
	TEST_ASSERT_EQUAL_INT(REPORT_THIS_MONTH, st.report_sel);
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_task_form_text_entry_does_not_trigger_navigation_shortcuts);
	RUN_TEST(test_task_form_accepts_croatian_letters_typed_byte_by_byte);
	RUN_TEST(test_task_form_cursor_and_backspace_move_by_character);
	RUN_TEST(test_task_form_drops_partial_character_interrupted_by_another_key);
	RUN_TEST(test_task_form_full_name_never_ends_in_half_a_character);
	RUN_TEST(test_project_form_accepts_non_ascii_and_backspaces_whole_character);
	RUN_TEST(test_project_switcher_query_accepts_non_ascii);
	RUN_TEST(test_task_form_tab_cycles_and_wraps);
	RUN_TEST(test_task_form_digit_selects_priority_only_when_priority_focused);
	RUN_TEST(test_task_form_enter_submits_from_either_field);
	RUN_TEST(test_task_form_esc_cancels_without_saving);
	RUN_TEST(test_navigate_tasks_enter_opens_edit_form_with_saved_values);
	RUN_TEST(test_navigate_tasks_r_key_opens_edit_form_same_as_enter);
	RUN_TEST(test_navigate_subtask_creation_on_top_level_selection);
	RUN_TEST(test_navigate_subtask_creation_on_subtask_selection_chains_under_same_parent);
	RUN_TEST(test_navigate_reorder_mode_moves_and_finishes);
	RUN_TEST(test_navigate_delete_confirms_then_suppresses_within_category);
	RUN_TEST(test_navigate_space_completion_requires_confirmation_for_subtasks);
	RUN_TEST(test_navigate_archive_completed_and_restore_contextual_a);
	RUN_TEST(test_pane_navigation_bounded_and_disabled_during_form);
	RUN_TEST(test_help_toggle);
	RUN_TEST(test_quit_returns_quit_action);
	RUN_TEST(test_project_switcher_filters_and_selects);
	RUN_TEST(test_move_task_esc_leaves_task_in_place);
	RUN_TEST(test_move_task_enter_moves_and_clamps_selection);
	RUN_TEST(test_move_key_ignored_on_subtask);
	RUN_TEST(test_help_scrolls_and_reopens_at_top);
	RUN_TEST(test_report_menu_opens_from_every_pane);
	RUN_TEST(test_report_menu_selects_period_and_returns_report_action);
	RUN_TEST(test_provisional_project_committed_atomically_on_first_task);
	RUN_TEST(test_navigate_projects_arrow_updates_current_project_live);
	RUN_TEST(test_navigate_projects_can_move_off_provisional_project);
	RUN_TEST(test_navigate_projects_delete_current_after_confirm_selects_next);
	RUN_TEST(test_navigate_projects_delete_current_without_prompt_selects_next);
	RUN_TEST(test_new_project_form_enter_shows_new_project_in_tasks);
	RUN_TEST(test_project_rename_highlight_follows_resorted_project);
	RUN_TEST(test_project_archive_current_shows_project_now_highlighted);
	RUN_TEST(test_project_switcher_highlight_accounts_for_provisional_row);
	RUN_TEST(test_task_delete_last_row_keeps_selection_on_new_last_row);
	RUN_TEST(test_archive_completed_clamps_selection_after_confirm);
	RUN_TEST(test_navigate_projects_n_key_is_not_new_project_shortcut_anymore);
	RUN_TEST(test_navigate_tasks_n_key_returns_edit_notes_action);
	RUN_TEST(test_navigate_notes_enter_and_c_key_trigger_actions);
	RUN_TEST(test_navigate_e_key_exports_only_from_tasks);
	RUN_TEST(test_task_form_new_task_focuses_created_task);
	RUN_TEST(test_reorder_task_sel_follows_moved_task);
	RUN_TEST(test_priority_change_task_sel_follows_reordered_task);
	RUN_TEST(test_task_form_priority_change_task_sel_follows_edited_task);
	return UNITY_END();
}
