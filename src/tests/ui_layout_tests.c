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
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_TASKS, 2, &geom);
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
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_PROJECTS, 2, &a);
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_NOTES, 2, &b);
	TEST_ASSERT_TRUE(a.projects_visible && a.notes_visible);
	TEST_ASSERT_TRUE(b.projects_visible && b.notes_visible);
}

void test_ui_layout_compact_shows_tasks_and_notes_by_default(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_TASKS, 2, &geom);
	TEST_ASSERT_FALSE(geom.projects_visible);
	TEST_ASSERT_TRUE(geom.notes_visible);
	TEST_ASSERT_TRUE(geom.tasks.w > 0);
	TEST_ASSERT_TRUE(geom.notes.w > 0);
	TEST_ASSERT_EQUAL_INT(0, geom.projects.w);
}

void test_ui_layout_compact_shows_notes_when_notes_focused(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_NOTES, 2, &geom);
	TEST_ASSERT_FALSE(geom.projects_visible);
	TEST_ASSERT_TRUE(geom.notes_visible);
}

void test_ui_layout_compact_shows_projects_and_tasks_when_projects_focused(void) {
	layout_geom_t geom;
	ui_layout_compute(24, 80, LAYOUT_COMPACT, FOCUS_PROJECTS, 2, &geom);
	TEST_ASSERT_TRUE(geom.projects_visible);
	TEST_ASSERT_FALSE(geom.notes_visible);
	TEST_ASSERT_TRUE(geom.projects.w > 0);
	TEST_ASSERT_TRUE(geom.tasks.w > 0);
	TEST_ASSERT_EQUAL_INT(0, geom.notes.w);
}

void test_ui_layout_minimal_shows_only_the_focused_pane(void) {
	layout_geom_t p, t, n;
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_PROJECTS, 2, &p);
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_TASKS, 2, &t);
	ui_layout_compute(12, 40, LAYOUT_MINIMAL, FOCUS_NOTES, 2, &n);

	TEST_ASSERT_TRUE(p.projects.w > 0 && p.tasks.w == 0 && p.notes.w == 0);
	TEST_ASSERT_TRUE(t.tasks.w > 0 && t.projects.w == 0 && t.notes.w == 0);
	TEST_ASSERT_TRUE(n.notes.w > 0 && n.projects.w == 0 && n.tasks.w == 0);
}

void test_ui_layout_minimal_hides_footer(void) {
	layout_geom_t geom;
	ui_layout_compute(31, 128, LAYOUT_MINIMAL, FOCUS_TASKS, 2, &geom);
	TEST_ASSERT_EQUAL_INT(0, geom.footer.h);
}

void test_ui_layout_short_window_hides_footer_even_when_wide(void) {
	layout_geom_t geom;
	ui_layout_compute(10, 128, LAYOUT_WIDE, FOCUS_TASKS, 2, &geom);
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
	/* "i Insert   d Delete" = 19 fits; all three (26) does not. */
	TEST_ASSERT_EQUAL_INT(2, ui_layout_footer_height(entries, 3, 20));
}

void test_ui_layout_footer_height_exceeds_max_when_nothing_fits(void) {
	hotkey_entry_t entries[] = {
		{ "i", "Insert" },
		{ "d", "Delete" },
		{ "q", "Quit" },
	};
	TEST_ASSERT_EQUAL_INT(3, ui_layout_footer_height(entries, 3, 5));
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

void test_ui_layout_footer_columns_fill_rows_and_align_by_longest_entry(void) {
	hotkey_entry_t entries[] = {
		{ "1/2/3", "Priority" },    /* row0 col0, width 14 */
		{ "o", "Order" },           /* row0 col1, width 7 */
		{ "a", "Archive done" },    /* row1 col0, width 14 */
		{ "A", "Show archived" },   /* row1 col1, width 15 */
		{ "q", "Quit" },            /* row2 col0, width 6 */
	};
	int widths[8];
	size_t ncols, nrows;
	/* 14 + 3 + 15 = 32 fits two columns; three would need 14+3+15+3+6 = 41. */
	ui_layout_footer_columns(entries, 5, 35, widths, 8, &ncols, &nrows);
	TEST_ASSERT_EQUAL_INT(2, (int)ncols);
	TEST_ASSERT_EQUAL_INT(3, (int)nrows);
	TEST_ASSERT_EQUAL_INT(14, widths[0]);
	TEST_ASSERT_EQUAL_INT(15, widths[1]);
}

void test_ui_layout_footer_columns_use_extra_width(void) {
	hotkey_entry_t entries[] = {
		{ "i", "Insert" }, { "d", "Delete" }, { "q", "Quit" }, { "?", "Help" },
	};
	int widths[8];
	size_t ncols, nrows;
	/* 8+3+8+3+6 = 28: three columns fit, so only the fourth wraps. */
	ui_layout_footer_columns(entries, 4, 30, widths, 8, &ncols, &nrows);
	TEST_ASSERT_EQUAL_INT(3, (int)ncols);
	TEST_ASSERT_EQUAL_INT(2, (int)nrows);
}

void test_ui_layout_footer_gets_exactly_the_rows_it_needs(void) {
	layout_geom_t geom;
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_TASKS, 3, &geom);
	TEST_ASSERT_EQUAL_INT(3, geom.footer.h);
	TEST_ASSERT_EQUAL_INT(28, geom.footer.y);
	TEST_ASSERT_EQUAL_INT(28, geom.tasks.h);
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_TASKS, 1, &geom);
	TEST_ASSERT_EQUAL_INT(1, geom.footer.h);
}

void test_ui_layout_footer_hidden_when_more_than_three_rows_needed(void) {
	layout_geom_t geom;
	ui_layout_compute(31, 128, LAYOUT_WIDE, FOCUS_TASKS, 4, &geom);
	TEST_ASSERT_EQUAL_INT(0, geom.footer.h);
	TEST_ASSERT_EQUAL_INT(31, geom.tasks.h);
}

void test_ui_layout_footer_hidden_rather_than_cut_when_window_short(void) {
	layout_geom_t geom;
	ui_layout_compute(19, 128, LAYOUT_WIDE, FOCUS_TASKS, 3, &geom);
	TEST_ASSERT_EQUAL_INT(0, geom.footer.h);
	ui_layout_compute(19, 128, LAYOUT_WIDE, FOCUS_TASKS, 2, &geom);
	TEST_ASSERT_EQUAL_INT(2, geom.footer.h);
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
	RUN_TEST(test_ui_layout_footer_height_exceeds_max_when_nothing_fits);
	RUN_TEST(test_ui_layout_footer_columns_fill_rows_and_align_by_longest_entry);
	RUN_TEST(test_ui_layout_footer_columns_use_extra_width);
	RUN_TEST(test_ui_layout_footer_gets_exactly_the_rows_it_needs);
	RUN_TEST(test_ui_layout_footer_hidden_when_more_than_three_rows_needed);
	RUN_TEST(test_ui_layout_footer_hidden_rather_than_cut_when_window_short);
	return UNITY_END();
}
