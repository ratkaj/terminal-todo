// lspdiag

#include <string.h>

#include <ui_layout.h>

#define LAYOUT_WIDE_MIN_COLS 100
#define LAYOUT_COMPACT_MIN_COLS 60
#define FOOTER_COLUMN_GAP 3
#define FOOTER_MAX_ROWS 3

static rect_t make_rect(int y, int x, int h, int w)
{
	rect_t r = { y, x, h, w };
	return r;
}

layout_tier_t ui_layout_tier(int rows, int cols)
{
	(void)rows;
	if (cols >= LAYOUT_WIDE_MIN_COLS)
		return LAYOUT_WIDE;
	if (cols >= LAYOUT_COMPACT_MIN_COLS)
		return LAYOUT_COMPACT;
	return LAYOUT_MINIMAL;
}

void ui_layout_compute(int rows, int cols, layout_tier_t tier,
	pane_focus_t focus, int footer_rows, layout_geom_t *out)
{
	if (out == NULL)
		return;
	memset(out, 0, sizeof(*out));
	if (rows < 0)
		rows = 0;
	if (cols < 0)
		cols = 0;

	/* The footer is either shown complete or hidden (the Help overlay then
	   lists the bindings): hidden in Minimal, when its entries need more
	   than FOOTER_MAX_ROWS rows at this width, or when the window is too
	   short to spare that many rows. */
	int max_footer_h;
	if (tier == LAYOUT_MINIMAL)
		max_footer_h = 0;
	else if (rows >= 20)
		max_footer_h = 3;
	else if (rows >= 18)
		max_footer_h = 2;
	else if (rows >= 14)
		max_footer_h = 1;
	else
		max_footer_h = 0;
	int footer_h = (footer_rows > 0 && footer_rows <= FOOTER_MAX_ROWS
		&& footer_rows <= max_footer_h) ? footer_rows : 0;

	int content_h = rows - footer_h;
	if (content_h < 0)
		content_h = 0;

	if (footer_h > 0)
		out->footer = make_rect(rows - footer_h, 0, footer_h, cols);

	if (tier == LAYOUT_WIDE) {
		int projects_w = cols / 5;
		if (projects_w < 18)
			projects_w = (cols >= 18) ? 18 : cols / 3;
		int notes_w = (cols * 3) / 10;
		if (notes_w < 20 && cols > 40)
			notes_w = 20;
		int tasks_w = cols - projects_w - notes_w - 2; /* 2 one-col separators */
		if (tasks_w < 0)
			tasks_w = 0;

		out->projects = make_rect(0, 0, content_h, projects_w);
		out->tasks = make_rect(0, projects_w + 1, content_h, tasks_w);
		out->notes = make_rect(0, projects_w + 1 + tasks_w + 1, content_h, notes_w);
		out->projects_visible = true;
		out->notes_visible = true;
	} else if (tier == LAYOUT_COMPACT) {
		if (focus == FOCUS_PROJECTS) {
			int projects_w = cols * 3 / 10;
			int tasks_w = cols - projects_w - 1;
			out->projects = make_rect(0, 0, content_h, projects_w);
			out->tasks = make_rect(0, projects_w + 1, content_h, tasks_w);
			out->projects_visible = true;
			out->notes_visible = false;
		} else {
			int tasks_w = cols * 7 / 10;
			int notes_w = cols - tasks_w - 1;
			out->tasks = make_rect(0, 0, content_h, tasks_w);
			out->notes = make_rect(0, tasks_w + 1, content_h, notes_w);
			out->projects_visible = false;
			out->notes_visible = true;
		}
	} else { /* LAYOUT_MINIMAL: exactly one pane, matching the focused pane. */
		rect_t full = make_rect(0, 0, content_h, cols);
		switch (focus) {
		case FOCUS_PROJECTS:
			out->projects = full;
			out->projects_visible = true;
			out->notes_visible = false;
			break;
		case FOCUS_NOTES:
			out->notes = full;
			out->projects_visible = false;
			out->notes_visible = true;
			break;
		case FOCUS_TASKS:
		default:
			out->tasks = full;
			out->projects_visible = false;
			out->notes_visible = false;
			break;
		}
	}
}

static int entry_width(const hotkey_entry_t *e)
{
	return (int)strlen(e->key) + 1 + (int)strlen(e->label);
}

/* Total width of an aligned layout with @p ncols columns, entries filled
   row by row; also writes each column's width if @p out_widths is non-NULL. */
static int aligned_width(const hotkey_entry_t *entries, size_t n, size_t ncols, int *out_widths)
{
	int total = 0;
	for (size_t c = 0; c < ncols; c++) {
		int col_w = 0;
		for (size_t i = c; i < n; i += ncols) {
			int w = entry_width(&entries[i]);
			if (w > col_w)
				col_w = w;
		}
		if (out_widths != NULL)
			out_widths[c] = col_w;
		total += col_w + (c > 0 ? FOOTER_COLUMN_GAP : 0);
	}
	return total;
}

/* The most columns (at most @p max_cols) whose aligned layout fits @p width;
   1 if even a single column is too wide. */
static size_t fitting_columns(const hotkey_entry_t *entries, size_t n, int width, size_t max_cols)
{
	size_t ncols = (n < max_cols) ? n : max_cols;
	while (ncols > 1 && aligned_width(entries, n, ncols, NULL) > width)
		ncols--;
	return ncols;
}

int ui_layout_footer_height(const hotkey_entry_t *entries, size_t n, int width)
{
	if (entries == NULL || n == 0)
		return 0;
	size_t ncols = fitting_columns(entries, n, width, n);
	if (aligned_width(entries, n, ncols, NULL) > width)
		return (int)n; /* Nothing fits; more rows than any footer may use. */
	return (int)((n + ncols - 1) / ncols);
}

void ui_layout_footer_columns(const hotkey_entry_t *entries, size_t n, int width,
	int *out_col_widths, size_t max_cols, size_t *out_ncols, size_t *out_nrows)
{
	if (entries == NULL || n == 0 || out_col_widths == NULL || max_cols == 0
			|| out_ncols == NULL || out_nrows == NULL)
		return;

	size_t ncols = fitting_columns(entries, n, width, max_cols);
	aligned_width(entries, n, ncols, out_col_widths);
	*out_ncols = ncols;
	*out_nrows = (n + ncols - 1) / ncols;
}
