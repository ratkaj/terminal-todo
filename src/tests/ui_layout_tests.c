// lspdiag

#include <logger.h>
#include <ui_layout.h>
#include <unity/unity.h>

void setUp(void) {
	logger_init(LOG_LVL_DEBUG, LOG_BACKEND_FILE, "/tmp/todo_ui_layout.log");
}

void tearDown(void) {
	logger_close();
}

void test_ui_layout_tier_reference_sizes(void) {
	TEST_ASSERT_EQUAL_INT(LAYOUT_WIDE, ui_layout_tier(31, 128));
	TEST_ASSERT_EQUAL_INT(LAYOUT_COMPACT, ui_layout_tier(24, 80));
	TEST_ASSERT_EQUAL_INT(LAYOUT_MINIMAL, ui_layout_tier(12, 40));
}

void test_ui_layout_wide_shows_all_three_panes(void) {
	layout_geom_t geom;
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_TASKS, &geom);
	TEST_ASSERT_TRUE(geom.projects_visible);
	TEST_ASSERT_TRUE(geom.notes_visible);
	TEST_ASSERT_TRUE(geom.projects.w > 0);
	TEST_ASSERT_TRUE(geom.tasks.w > 0);
	TEST_ASSERT_TRUE(geom.notes.w > 0);
	/* Panes must not overlap: each starts after the previous one's end + separator. */
	TEST_ASSERT_TRUE(geom.tasks.x >= geom.projects.x + geom.projects.w);
	TEST_ASSERT_TRUE(geom.notes.x >= geom.tasks.x + geom.tasks.w);
}

void test_ui_layout_wide_ignores_focus(void) {
	layout_geom_t a, b;
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_PROJECTS, &a);
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_NOTES, &b);
	TEST_ASSERT_TRUE(a.projects_visible && a.notes_visible);
	TEST_ASSERT_TRUE(b.projects_visible && b.notes_visible);
}

void test_ui_layout_compact_shows_tasks_and_notes_by_default(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_TASKS, &geom);
	TEST_ASSERT_FALSE(geom.projects_visible);
	TEST_ASSERT_TRUE(geom.notes_visible);
	TEST_ASSERT_TRUE(geom.tasks.w > 0);
	TEST_ASSERT_TRUE(geom.notes.w > 0);
	TEST_ASSERT_EQUAL_INT(0, geom.projects.w);
}

void test_ui_layout_compact_shows_notes_when_notes_focused(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_NOTES, &geom);
	TEST_ASSERT_FALSE(geom.projects_visible);
	TEST_ASSERT_TRUE(geom.notes_visible);
}

void test_ui_layout_compact_shows_projects_and_tasks_when_projects_focused(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_PROJECTS, &geom);
	TEST_ASSERT_TRUE(geom.projects_visible);
	TEST_ASSERT_FALSE(geom.notes_visible);
	TEST_ASSERT_TRUE(geom.projects.w > 0);
	TEST_ASSERT_TRUE(geom.tasks.w > 0);
	TEST_ASSERT_EQUAL_INT(0, geom.notes.w);
}

void test_ui_layout_minimal_shows_only_the_focused_pane(void) {
	layout_geom_t p, t, n;
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_PROJECTS, &p);
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_TASKS, &t);
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_NOTES, &n);

	TEST_ASSERT_TRUE(p.projects.w > 0 && p.tasks.w == 0 && p.notes.w == 0);
	TEST_ASSERT_TRUE(t.tasks.w > 0 && t.projects.w == 0 && t.notes.w == 0);
	TEST_ASSERT_TRUE(n.notes.w > 0 && n.projects.w == 0 && n.tasks.w == 0);
}

void test_ui_layout_minimal_hides_footer(void) {
	layout_geom_t geom;
	ui_layout_compute(31, 128, LAYOUT_MINIMAL, FOCUS_TASKS, &geom);
	TEST_ASSERT_EQUAL_INT(0, geom.footer.h);
}

void test_ui_layout_short_window_hides_footer_even_when_wide(void) {
	layout_geom_t geom;
	ui_layout_compute(10, 128, LAYOUT_WIDE, FOCUS_TASKS, &geom);
	TEST_ASSERT_EQUAL_INT(0, geom.footer.h);
}

void test_ui_layout_footer_height_fits_one_row(void) {
	hotkey_entry_t entries[] = {
		{ "i", "Insert" },
		{ "d", "Delete" },
		{ "q", "Quit" },
	};
	TEST_ASSERT_EQUAL_INT(1, ui_layout_footer_height(entries, 3, 80));
}

void test_ui_layout_footer_height_wraps_to_two_rows_when_too_narrow(void) {
	hotkey_entry_t entries[] = {
		{ "i", "Insert" },
		{ "d", "Delete" },
		{ "q", "Quit" },
	};
	TEST_ASSERT_EQUAL_INT(2, ui_layout_footer_height(entries, 3, 10));
}

void test_ui_layout_footer_columns_single_row(void) {
	hotkey_entry_t entries[] = {
		{ "i", "Insert" },
		{ "d", "Delete" },
	};
	int widths[8];
	size_t ncols, nrows;
	ui_layout_footer_columns(entries, 2, 80, widths, 8, &ncols, &nrows);
	TEST_ASSERT_EQUAL_INT(1, (int)nrows);
	TEST_ASSERT_EQUAL_INT(2, (int)ncols);
	TEST_ASSERT_EQUAL_INT(8, widths[0]);  /* "i Insert" */
	TEST_ASSERT_EQUAL_INT(8, widths[1]);  /* "d Delete" */
}

void test_ui_layout_footer_columns_two_rows_align_by_longest_entry(void) {
	hotkey_entry_t entries[] = {
		{ "1/2/3", "Priority" },  /* row1 col0, width 14 */
		{ "o", "Order" },         /* row1 col1, width 7 */
		{ "a", "Archive completed" }, /* row2 col0, width 19 */
		{ "A", "Display archived" },  /* row2 col1, width 19 */
	};
	int widths[8];
	size_t ncols, nrows;
	ui_layout_footer_columns(entries, 4, 5 /* force two rows */, widths, 8, &ncols, &nrows);
	TEST_ASSERT_EQUAL_INT(2, (int)nrows);
	TEST_ASSERT_EQUAL_INT(2, (int)ncols);
	TEST_ASSERT_EQUAL_INT(19, widths[0]); /* max("1/2/3 Priority"=14, "a Archive completed"=19) */
	TEST_ASSERT_EQUAL_INT(18, widths[1]); /* max("o Order"=7, "A Display archived"=18) */
}

int main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_ui_layout_tier_reference_sizes);
	RUN_TEST(test_ui_layout_wide_shows_all_three_panes);
	RUN_TEST(test_ui_layout_wide_ignores_focus);
	RUN_TEST(test_ui_layout_compact_shows_tasks_and_notes_by_default);
	RUN_TEST(test_ui_layout_compact_shows_notes_when_notes_focused);
	RUN_TEST(test_ui_layout_compact_shows_projects_and_tasks_when_projects_focused);
	RUN_TEST(test_ui_layout_minimal_shows_only_the_focused_pane);
	RUN_TEST(test_ui_layout_minimal_hides_footer);
	RUN_TEST(test_ui_layout_short_window_hides_footer_even_when_wide);
	RUN_TEST(test_ui_layout_footer_height_fits_one_row);
	RUN_TEST(test_ui_layout_footer_height_wraps_to_two_rows_when_too_narrow);
	RUN_TEST(test_ui_layout_footer_columns_single_row);
	RUN_TEST(test_ui_layout_footer_columns_two_rows_align_by_longest_entry);
	return UNITY_END();
}
