// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <report.h>
#include <storage.h>
#include <strbuf.h>

static const char *LABELS[REPORT_PERIOD_COUNT] = {
	"This week", "Last week", "This month", "Last month",
};

const char *report_period_label(report_period_t period)
{
	if (period < 0 || period >= REPORT_PERIOD_COUNT)
		return "";
	return LABELS[period];
}

/* Local midnight of @p tm's day shifted by @p days / @p months; mktime()
   normalizes out-of-range fields, and tm_isdst = -1 lets it pick the right
   offset on either side of a DST change. */
static time_t local_midnight(struct tm tm, int days, int months)
{
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	tm.tm_mday += days;
	tm.tm_mon += months;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

void report_period_range(report_period_t period, time_t now, time_t *out_start, time_t *out_end)
{
	struct tm tm;
	localtime_r(&now, &tm);
	int since_monday = (tm.tm_wday + 6) % 7;

	struct tm month = tm;
	month.tm_mday = 1;

	time_t start, end;
	switch (period) {
	case REPORT_LAST_WEEK:
		start = local_midnight(tm, -since_monday - 7, 0);
		end = local_midnight(tm, -since_monday, 0);
		break;
	case REPORT_THIS_MONTH:
		start = local_midnight(month, 0, 0);
		end = now + 1;
		break;
	case REPORT_LAST_MONTH:
		start = local_midnight(month, 0, -1);
		end = local_midnight(month, 0, 0);
		break;
	case REPORT_THIS_WEEK:
	default:
		start = local_midnight(tm, -since_monday, 0);
		end = now + 1;
		break;
	}
	if (out_start != NULL)
		*out_start = start;
	if (out_end != NULL)
		*out_end = end;
}

static void format_day(time_t t, char *out, size_t cap)
{
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(out, cap, "%Y-%m-%d", &tm);
}

static bool is_hit(const task_t *t, time_t start, time_t end)
{
	return t->status == TASK_STATUS_COMPLETED && t->completed_at >= start
		&& t->completed_at < end;
}

static void append_row(strbuf_t *sb, const task_t *t, bool hit)
{
	char day[16] = "";
	if (hit)
		format_day(t->completed_at, day, sizeof(day));
	sb_appendf(sb, "  %-10s  %s[%c] P%d  %s%s\n", day, t->parent_id != 0 ? "    " : "",
		t->status == TASK_STATUS_COMPLETED ? 'x' : ' ', (int)t->priority, t->title,
		t->archived ? "  (archived)" : "");
}

int report_completed_text(report_period_t period, time_t now, char **out_text)
{
	RETURN_ERR_IF(out_text == NULL, "report_completed_text: out_text is NULL");
	RETURN_ERR_IF(period < 0 || period >= REPORT_PERIOD_COUNT,
		"report_completed_text: invalid period %d", (int)period);

	time_t start, end;
	report_period_range(period, now, &start, &end);

	task_t *rows = NULL;
	size_t n = 0;
	RETURN_ERR_IF(storage_task_list_completed_between(start, end, &rows, &n) != RT_SUCCESS,
		"report_completed_text: listing completed tasks failed");

	/* Rows arrive grouped by project; count hits and projects up front for
	   the header. */
	int total = 0, projects = 0;
	for (size_t i = 0; i < n; i++) {
		if (i == 0 || rows[i].project_id != rows[i - 1].project_id)
			projects++;
		if (is_hit(&rows[i], start, end))
			total++;
	}

	char from[16], to[16], stamp[32];
	format_day(start, from, sizeof(from));
	bool so_far = (period == REPORT_THIS_WEEK || period == REPORT_THIS_MONTH);
	format_day(so_far ? now : end - 1, to, sizeof(to));
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &tm);

	char title[32];
	snprintf(title, sizeof(title), "%s", report_period_label(period));
	title[0] = (char)(title[0] - 'A' + 'a');

	strbuf_t sb = {0};
	sb_appendf(&sb, "Completed %s\n", title);
	sb_appendf(&sb, "Period:    %s to %s%s\n", from, to, so_far ? " (so far)" : "");
	sb_appendf(&sb, "Generated: %s\n", stamp);
	sb_appendf(&sb, "Total:     %d task%s in %d project%s\n\n", total, total == 1 ? "" : "s",
		projects, projects == 1 ? "" : "s");

	if (n == 0)
		sb_appendf(&sb, "(no tasks completed)\n");

	for (size_t i = 0; i < n;) {
		size_t j = i;
		int hits = 0;
		while (j < n && rows[j].project_id == rows[i].project_id) {
			if (is_hit(&rows[j], start, end))
				hits++;
			j++;
		}

		project_t p;
		bool have_project = (storage_project_get(rows[i].project_id, &p) == RT_SUCCESS);
		if (i > 0)
			sb_appendf(&sb, "\n");
		sb_appendf(&sb, "%s (%d%s)\n", have_project ? p.display_name : "?", hits,
			have_project && p.archived ? ", archived" : "");
		if (have_project)
			project_model_free(&p);

		for (size_t k = i; k < j; k++)
			append_row(&sb, &rows[k], is_hit(&rows[k], start, end));
		i = j;
	}

	storage_task_array_free(rows, n);

	if (sb.failed) {
		LERR("report_completed_text: out of memory");
		free(sb.buf);
		return RT_ERROR;
	}
	*out_text = sb.buf;
	return RT_SUCCESS;
}
