// lspdiag

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <sqlite3.h>

#include <common.h>
#include <storage.h>

static sqlite3 *db = NULL;

#define PATH_MAX_LOCAL 4096
/* How long a write waits for another connection (e.g. a second instance)
   to release its lock before failing with SQLITE_BUSY. */
#define STORAGE_BUSY_TIMEOUT_MS 2000

#define TASK_SELECT_COLUMNS \
	"id, project_id, parent_id, title, notes, status, priority, manual_order, " \
	"archived, created_at, completed_at, " \
	"(CASE WHEN archived=1 THEN 2 WHEN status=1 THEN 1 ELSE 0 END) AS state"

static const char *SCHEMA_SQL =
	"CREATE TABLE IF NOT EXISTS project ("
	"    id             INTEGER PRIMARY KEY AUTOINCREMENT,"
	"    display_name   TEXT NOT NULL,"
	"    canonical_path TEXT UNIQUE,"
	"    archived       INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0,1)),"
	"    builtin        INTEGER NOT NULL DEFAULT 0 CHECK (builtin IN (0,1))"
	");"
	"CREATE TABLE IF NOT EXISTS task ("
	"    id            INTEGER PRIMARY KEY AUTOINCREMENT,"
	"    project_id    INTEGER NOT NULL REFERENCES project(id) ON DELETE CASCADE,"
	"    parent_id     INTEGER REFERENCES task(id) ON DELETE CASCADE,"
	"    title         TEXT NOT NULL,"
	"    notes         TEXT,"
	"    status        INTEGER NOT NULL DEFAULT 0 CHECK (status IN (0,1)),"
	"    priority      INTEGER NOT NULL DEFAULT 3 CHECK (priority IN (1,2,3)),"
	"    manual_order  INTEGER NOT NULL DEFAULT 0,"
	"    archived      INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0,1)),"
	"    created_at    INTEGER NOT NULL,"
	"    completed_at  INTEGER"
	");"
	"CREATE INDEX IF NOT EXISTS idx_task_project_toplevel ON task(project_id, parent_id);"
	"CREATE INDEX IF NOT EXISTS idx_task_parent ON task(parent_id);"
	"CREATE TRIGGER IF NOT EXISTS task_no_double_nesting "
	"BEFORE INSERT ON task "
	"WHEN NEW.parent_id IS NOT NULL "
	"BEGIN "
	"    SELECT RAISE(ABORT, 'subtasks cannot themselves have subtasks') "
	"    WHERE (SELECT parent_id FROM task WHERE id = NEW.parent_id) IS NOT NULL;"
	"END;"
	"INSERT INTO project (display_name, canonical_path, archived, builtin) "
	"SELECT 'Today', NULL, 0, 1 WHERE NOT EXISTS "
	"    (SELECT 1 FROM project WHERE display_name = 'Today' AND builtin = 1);"
	"INSERT INTO project (display_name, canonical_path, archived, builtin) "
	"SELECT 'This Week', NULL, 0, 1 WHERE NOT EXISTS "
	"    (SELECT 1 FROM project WHERE display_name = 'This Week' AND builtin = 1);"
	"INSERT INTO project (display_name, canonical_path, archived, builtin) "
	"SELECT 'Inbox', NULL, 0, 1 WHERE NOT EXISTS "
	"    (SELECT 1 FROM project WHERE display_name = 'Inbox' AND builtin = 1);";

/* ---- small shared helpers ---- */

static void bind_text_or_null(sqlite3_stmt *stmt, int idx, const char *s)
{
	if (s == NULL)
		sqlite3_bind_null(stmt, idx);
	else
		sqlite3_bind_text(stmt, idx, s, -1, SQLITE_TRANSIENT);
}

/** UPDATE/DELETE statements shaped "... WHERE id = ?1" (or similarly one int64 param). */
static int exec_with_int64(const char *sql, int64_t param)
{
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"exec_with_int64: prepare failed for '%s': %s", sql, sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)param);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	RETURN_ERR_IF(rc != SQLITE_DONE,
		"exec_with_int64: step failed for '%s': %s", sql, sqlite3_errmsg(db));
	return RT_SUCCESS;
}

static int scalar_count(const char *sql, int64_t param)
{
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"scalar_count: prepare failed for '%s': %s", sql, sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)param);
	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		LERR("scalar_count: step failed for '%s': %s", sql, sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return RT_ERROR;
	}
	int count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return count;
}

static void escape_like(const char *in, char *out, size_t out_cap)
{
	size_t j = 0;
	for (size_t i = 0; in[i] != '\0' && j + 2 < out_cap; i++) {
		char c = in[i];
		if (c == '%' || c == '_' || c == '\\')
			out[j++] = '\\';
		out[j++] = c;
	}
	out[j] = '\0';
}

/* ---- lifecycle ---- */

static int storage_default_path(char *out, size_t out_cap)
{
	const char *home = getenv("HOME");
	RETURN_ERR_IF(home == NULL, "storage_default_path: HOME is not set");

	char dir[PATH_MAX_LOCAL];
	int n = snprintf(dir, sizeof(dir), "%s/.local", home);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(dir), "storage_default_path: HOME too long");
	if (mkdir(dir, 0700) != 0 && errno != EEXIST)
		LWARN("storage_default_path: mkdir %s: %s", dir, strerror(errno));

	n = snprintf(dir, sizeof(dir), "%s/.local/share", home);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(dir), "storage_default_path: HOME too long");
	if (mkdir(dir, 0700) != 0 && errno != EEXIST)
		LWARN("storage_default_path: mkdir %s: %s", dir, strerror(errno));

	n = snprintf(dir, sizeof(dir), "%s/.local/share/todo", home);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(dir), "storage_default_path: HOME too long");
	RETURN_ERR_IF(mkdir(dir, 0700) != 0 && errno != EEXIST,
		"storage_default_path: mkdir %s: %s", dir, strerror(errno));

	n = snprintf(out, out_cap, "%s/todo.db", dir);
	RETURN_ERR_IF(n < 0 || (size_t)n >= out_cap, "storage_default_path: path too long");
	return RT_SUCCESS;
}

int storage_open(const char *db_path)
{
	RETURN_ERR_IF(db != NULL, "storage_open: already open");

	char path_buf[PATH_MAX_LOCAL];
	const char *path = db_path;
	if (path == NULL) {
		RETURN_ERR_IF(storage_default_path(path_buf, sizeof(path_buf)) != RT_SUCCESS,
			"storage_open: cannot determine default db path");
		path = path_buf;
	}

	if (sqlite3_open(path, &db) != SQLITE_OK) {
		LERR("storage_open: sqlite3_open(%s) failed: %s", path, sqlite3_errmsg(db));
		sqlite3_close(db);
		db = NULL;
		return RT_ERROR;
	}

	sqlite3_busy_timeout(db, STORAGE_BUSY_TIMEOUT_MS);

	char *errmsg = NULL;
	if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, &errmsg) != SQLITE_OK) {
		LERR("storage_open: enabling foreign_keys failed: %s", errmsg);
		sqlite3_free(errmsg);
		sqlite3_close(db);
		db = NULL;
		return RT_ERROR;
	}

	if (sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &errmsg) != SQLITE_OK) {
		LERR("storage_open: schema init failed: %s", errmsg);
		sqlite3_free(errmsg);
		sqlite3_close(db);
		db = NULL;
		return RT_ERROR;
	}

	return RT_SUCCESS;
}

void storage_close(void)
{
	if (db == NULL)
		return;
	if (!sqlite3_get_autocommit(db)) {
		LWARN("storage_close: transaction still open; rolling back");
		storage_rollback();
	}
	sqlite3_close(db);
	db = NULL;
}

static int exec_logged(const char *sql, const char *who)
{
	char *errmsg = NULL;
	int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) {
		LERR("%s: %s", who, errmsg ? errmsg : sqlite3_errmsg(db));
		sqlite3_free(errmsg);
		return RT_ERROR;
	}
	return RT_SUCCESS;
}

int storage_begin(void)
{
	RETURN_ERR_IF(db == NULL, "storage_begin: storage not open");
	/* IMMEDIATE takes the write lock up front, so a second instance makes
	   BEGIN wait (busy timeout) instead of failing later at COMMIT. */
	return exec_logged("BEGIN IMMEDIATE;", "storage_begin");
}

int storage_commit(void)
{
	RETURN_ERR_IF(db == NULL, "storage_commit: storage not open");
	if (exec_logged("COMMIT;", "storage_commit") == RT_SUCCESS)
		return RT_SUCCESS;
	/* A failed COMMIT (e.g. SQLITE_BUSY) leaves the transaction open; roll
	   it back so later writes are not silently folded into it and lost. */
	if (!sqlite3_get_autocommit(db))
		storage_rollback();
	return RT_ERROR;
}

int storage_rollback(void)
{
	RETURN_ERR_IF(db == NULL, "storage_rollback: storage not open");
	return exec_logged("ROLLBACK;", "storage_rollback");
}

/* ---- row <-> struct mapping ---- */

static void row_to_task(sqlite3_stmt *stmt, task_t *t)
{
	memset(t, 0, sizeof(*t));
	t->id = sqlite3_column_int64(stmt, 0);
	t->project_id = sqlite3_column_int64(stmt, 1);
	t->parent_id = (sqlite3_column_type(stmt, 2) == SQLITE_NULL)
		? 0 : sqlite3_column_int64(stmt, 2);
	const unsigned char *title = sqlite3_column_text(stmt, 3);
	snprintf(t->title, sizeof(t->title), "%s", title ? (const char *)title : "");
	const unsigned char *notes = sqlite3_column_text(stmt, 4);
	t->notes = notes ? strdup((const char *)notes) : NULL;
	t->status = (task_status_t)sqlite3_column_int(stmt, 5);
	t->priority = (priority_t)sqlite3_column_int(stmt, 6);
	t->manual_order = (long)sqlite3_column_int64(stmt, 7);
	t->archived = sqlite3_column_int(stmt, 8) != 0;
	t->created_at = (time_t)sqlite3_column_int64(stmt, 9);
	t->completed_at = (sqlite3_column_type(stmt, 10) == SQLITE_NULL)
		? 0 : (time_t)sqlite3_column_int64(stmt, 10);
	t->state = (task_state_t)sqlite3_column_int(stmt, 11);
}

static void row_to_project(sqlite3_stmt *stmt, project_t *p)
{
	memset(p, 0, sizeof(*p));
	p->id = sqlite3_column_int64(stmt, 0);
	const unsigned char *name = sqlite3_column_text(stmt, 1);
	snprintf(p->display_name, sizeof(p->display_name), "%s", name ? (const char *)name : "");
	const unsigned char *path = sqlite3_column_text(stmt, 2);
	p->canonical_path = path ? strdup((const char *)path) : NULL;
	p->archived = sqlite3_column_int(stmt, 3) != 0;
	p->builtin = sqlite3_column_int(stmt, 4) != 0;
}

static int collect_tasks(sqlite3_stmt *stmt, task_t **out_arr, size_t *out_n)
{
	size_t cap = 8, n = 0;
	task_t *arr = malloc(cap * sizeof(*arr));
	RETURN_ERR_IF(arr == NULL, "collect_tasks: out of memory");

	int rc;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (n == cap) {
			cap *= 2;
			task_t *tmp = realloc(arr, cap * sizeof(*arr));
			if (tmp == NULL) {
				LERR("collect_tasks: out of memory");
				storage_task_array_free(arr, n);
				return RT_ERROR;
			}
			arr = tmp;
		}
		row_to_task(stmt, &arr[n]);
		n++;
	}
	if (rc != SQLITE_DONE) {
		LERR("collect_tasks: step failed: %s", sqlite3_errmsg(db));
		storage_task_array_free(arr, n);
		return RT_ERROR;
	}
	*out_arr = arr;
	*out_n = n;
	return RT_SUCCESS;
}

static int collect_projects(sqlite3_stmt *stmt, project_t **out_arr, size_t *out_n)
{
	size_t cap = 8, n = 0;
	project_t *arr = malloc(cap * sizeof(*arr));
	RETURN_ERR_IF(arr == NULL, "collect_projects: out of memory");

	int rc;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (n == cap) {
			cap *= 2;
			project_t *tmp = realloc(arr, cap * sizeof(*arr));
			if (tmp == NULL) {
				LERR("collect_projects: out of memory");
				storage_project_array_free(arr, n);
				return RT_ERROR;
			}
			arr = tmp;
		}
		row_to_project(stmt, &arr[n]);
		n++;
	}
	if (rc != SQLITE_DONE) {
		LERR("collect_projects: step failed: %s", sqlite3_errmsg(db));
		storage_project_array_free(arr, n);
		return RT_ERROR;
	}
	*out_arr = arr;
	*out_n = n;
	return RT_SUCCESS;
}

void storage_task_array_free(task_t *arr, size_t n)
{
	if (arr == NULL)
		return;
	for (size_t i = 0; i < n; i++)
		task_model_free(&arr[i]);
	free(arr);
}

void storage_project_array_free(project_t *arr, size_t n)
{
	if (arr == NULL)
		return;
	for (size_t i = 0; i < n; i++)
		project_model_free(&arr[i]);
	free(arr);
}

/* ---- projects ---- */

int storage_project_insert(const project_t *p, int64_t *out_id)
{
	RETURN_ERR_IF(db == NULL || p == NULL, "storage_project_insert: invalid arguments");

	static const char *sql =
		"INSERT INTO project (display_name, canonical_path, archived, builtin) "
		"VALUES (?1, ?2, ?3, ?4)";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_insert: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_text(stmt, 1, p->display_name, -1, SQLITE_TRANSIENT);
	bind_text_or_null(stmt, 2, p->canonical_path);
	sqlite3_bind_int(stmt, 3, p->archived ? 1 : 0);
	sqlite3_bind_int(stmt, 4, p->builtin ? 1 : 0);

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		LERR("storage_project_insert: step failed: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return RT_ERROR;
	}
	int64_t id = sqlite3_last_insert_rowid(db);
	sqlite3_finalize(stmt);

	if (out_id != NULL)
		*out_id = id;
	return RT_SUCCESS;
}

int storage_project_update(const project_t *p)
{
	RETURN_ERR_IF(db == NULL || p == NULL, "storage_project_update: invalid arguments");

	static const char *sql =
		"UPDATE project SET display_name = ?2, canonical_path = ?3, archived = ?4, builtin = ?5 "
		"WHERE id = ?1";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_update: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)p->id);
	sqlite3_bind_text(stmt, 2, p->display_name, -1, SQLITE_TRANSIENT);
	bind_text_or_null(stmt, 3, p->canonical_path);
	sqlite3_bind_int(stmt, 4, p->archived ? 1 : 0);
	sqlite3_bind_int(stmt, 5, p->builtin ? 1 : 0);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	RETURN_ERR_IF(rc != SQLITE_DONE, "storage_project_update: step failed: %s", sqlite3_errmsg(db));
	return RT_SUCCESS;
}

int storage_project_get(int64_t id, project_t *out)
{
	RETURN_ERR_IF(db == NULL || out == NULL, "storage_project_get: invalid arguments");

	static const char *sql =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project WHERE id = ?1";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_get: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		LERR_IF(rc != SQLITE_DONE, "storage_project_get: step failed: %s", sqlite3_errmsg(db));
		return RT_ERROR;
	}
	row_to_project(stmt, out);
	sqlite3_finalize(stmt);
	return RT_SUCCESS;
}

int storage_project_find_by_path(const char *canonical_path, project_t *out)
{
	RETURN_ERR_IF(db == NULL || canonical_path == NULL || out == NULL,
		"storage_project_find_by_path: invalid arguments");

	static const char *sql =
		"SELECT id, display_name, canonical_path, archived, builtin "
		"FROM project WHERE canonical_path = ?1";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_find_by_path: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_text(stmt, 1, canonical_path, -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		return RT_ERROR;
	}
	row_to_project(stmt, out);
	sqlite3_finalize(stmt);
	return RT_SUCCESS;
}

int storage_project_list(bool include_archived, project_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || out_arr == NULL || out_n == NULL,
		"storage_project_list: invalid arguments");

	static const char *sql_all =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project "
		"ORDER BY builtin DESC, archived ASC, display_name ASC, id ASC";
	static const char *sql_active =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project "
		"WHERE archived = 0 "
		"ORDER BY builtin DESC, archived ASC, display_name ASC, id ASC";

	sqlite3_stmt *stmt = NULL;
	const char *sql = include_archived ? sql_all : sql_active;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_list: prepare failed: %s", sqlite3_errmsg(db));

	int rc = collect_projects(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_project_list_move_targets(int64_t exclude_id, project_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || out_arr == NULL || out_n == NULL,
		"storage_project_list_move_targets: invalid arguments");

	/* Same ordering as storage_project_list(), minus archived projects and
	   the task's current project. */
	static const char *sql =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project "
		"WHERE archived = 0 AND id != ?1 "
		"ORDER BY builtin DESC, display_name ASC, id ASC";

	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_list_move_targets: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)exclude_id);

	int rc = collect_projects(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_project_search(const char *query, bool include_archived,
	project_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || query == NULL || out_arr == NULL || out_n == NULL,
		"storage_project_search: invalid arguments");

	static const char *sql_all =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project "
		"WHERE display_name LIKE '%' || ?1 || '%' ESCAPE '\\' "
		"ORDER BY archived ASC, display_name ASC, id ASC";
	static const char *sql_active =
		"SELECT id, display_name, canonical_path, archived, builtin FROM project "
		"WHERE archived = 0 AND display_name LIKE '%' || ?1 || '%' ESCAPE '\\' "
		"ORDER BY archived ASC, display_name ASC, id ASC";

	char escaped[PROJECT_NAME_MAX * 2];
	escape_like(query, escaped, sizeof(escaped));

	sqlite3_stmt *stmt = NULL;
	const char *sql = include_archived ? sql_all : sql_active;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_search: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_text(stmt, 1, escaped, -1, SQLITE_TRANSIENT);

	int rc = collect_projects(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_project_task_count(int64_t project_id)
{
	RETURN_ERR_IF(db == NULL, "storage_project_task_count: storage not open");
	/* Counts top-level, non-archived tasks (open + completed). Subtasks are
	   excluded per the "counts include top-level tasks only" rule. */
	static const char *sql =
		"SELECT COUNT(*) FROM task WHERE project_id = ?1 AND parent_id IS NULL AND archived = 0";
	return scalar_count(sql, project_id);
}

int storage_project_count_archived(void)
{
	RETURN_ERR_IF(db == NULL, "storage_project_count_archived: storage not open");

	sqlite3_stmt *stmt = NULL;
	static const char *sql = "SELECT COUNT(*) FROM project WHERE archived = 1";
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_project_count_archived: prepare failed: %s", sqlite3_errmsg(db));

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		LERR("storage_project_count_archived: step failed: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return RT_ERROR;
	}
	int count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return count;
}

int storage_project_delete_cascade(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_project_delete_cascade: storage not open");
	return exec_with_int64("DELETE FROM project WHERE id = ?1", id);
}

int storage_project_clear_tasks(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_project_clear_tasks: storage not open");
	return exec_with_int64("DELETE FROM task WHERE project_id = ?1", id);
}

/* ---- tasks ---- */

int storage_task_list_top_level(int64_t project_id, bool include_archived,
	task_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || out_arr == NULL || out_n == NULL,
		"storage_task_list_top_level: invalid arguments");

	static const char *sql_all =
		"SELECT " TASK_SELECT_COLUMNS " FROM task "
		"WHERE project_id = ?1 AND parent_id IS NULL "
		"ORDER BY state, priority, manual_order, id";
	static const char *sql_active =
		"SELECT " TASK_SELECT_COLUMNS " FROM task "
		"WHERE project_id = ?1 AND parent_id IS NULL AND archived = 0 "
		"ORDER BY state, priority, manual_order, id";

	sqlite3_stmt *stmt = NULL;
	const char *sql = include_archived ? sql_all : sql_active;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_list_top_level: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)project_id);

	int rc = collect_tasks(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_task_list_subtasks(int64_t parent_id, bool include_archived,
	task_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || out_arr == NULL || out_n == NULL,
		"storage_task_list_subtasks: invalid arguments");

	static const char *sql_all =
		"SELECT " TASK_SELECT_COLUMNS " FROM task "
		"WHERE parent_id = ?1 "
		"ORDER BY state, priority, manual_order, id";
	static const char *sql_active =
		"SELECT " TASK_SELECT_COLUMNS " FROM task "
		"WHERE parent_id = ?1 AND archived = 0 "
		"ORDER BY state, priority, manual_order, id";

	sqlite3_stmt *stmt = NULL;
	const char *sql = include_archived ? sql_all : sql_active;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_list_subtasks: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)parent_id);

	int rc = collect_tasks(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_task_get(int64_t id, task_t *out)
{
	RETURN_ERR_IF(db == NULL || out == NULL, "storage_task_get: invalid arguments");

	static const char *sql = "SELECT " TASK_SELECT_COLUMNS " FROM task WHERE id = ?1";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_get: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		LERR_IF(rc != SQLITE_DONE, "storage_task_get: step failed: %s", sqlite3_errmsg(db));
		return RT_ERROR;
	}
	row_to_task(stmt, out);
	sqlite3_finalize(stmt);
	return RT_SUCCESS;
}

int storage_task_insert(int64_t project_id, int64_t parent_id, const char *title,
	priority_t priority, task_t *out)
{
	RETURN_ERR_IF(db == NULL || title == NULL, "storage_task_insert: invalid arguments");

	/* manual_order appends to the end of the destination ACTIVE/priority
	   group in the same statement - no C-side loop. */
	static const char *sql =
		"INSERT INTO task (project_id, parent_id, title, status, priority, "
		"                   manual_order, archived, created_at, completed_at) "
		"VALUES (?1, ?2, ?3, 0, ?4, "
		"    (SELECT COALESCE(MAX(manual_order), 0) + 10 FROM task "
		"     WHERE project_id = ?1 AND parent_id IS ?2 AND archived = 0 "
		"       AND status = 0 AND priority = ?4), "
		"    0, ?5, NULL)";

	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_insert: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)project_id);
	if (parent_id == 0)
		sqlite3_bind_null(stmt, 2);
	else
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)parent_id);
	sqlite3_bind_text(stmt, 3, title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, (int)priority);
	sqlite3_bind_int64(stmt, 5, (sqlite3_int64)time(NULL));

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		LERR("storage_task_insert: step failed: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return RT_ERROR;
	}
	int64_t id = sqlite3_last_insert_rowid(db);
	sqlite3_finalize(stmt);

	if (out != NULL)
		return storage_task_get(id, out);
	return RT_SUCCESS;
}

int storage_task_update_fields(int64_t id, const char *title, const char *notes)
{
	RETURN_ERR_IF(db == NULL, "storage_task_update_fields: storage not open");

	static const char *sql =
		"UPDATE task SET title = COALESCE(?2, title), notes = COALESCE(?3, notes) WHERE id = ?1";
	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_update_fields: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
	bind_text_or_null(stmt, 2, title);
	bind_text_or_null(stmt, 3, notes);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	RETURN_ERR_IF(rc != SQLITE_DONE,
		"storage_task_update_fields: step failed: %s", sqlite3_errmsg(db));
	return RT_SUCCESS;
}

int storage_task_set_priority(int64_t id, priority_t new_priority)
{
	RETURN_ERR_IF(db == NULL, "storage_task_set_priority: storage not open");

	/* Appends to the end of the new priority's state group via a correlated
	   subquery keyed on this row's own project/parent/archived/status. */
	static const char *sql =
		"UPDATE task SET priority = ?2, manual_order = ("
		"    SELECT COALESCE(MAX(t2.manual_order), 0) + 10 FROM task t2"
		"    WHERE t2.project_id = task.project_id"
		"      AND t2.parent_id IS task.parent_id"
		"      AND t2.archived = task.archived"
		"      AND t2.status = task.status"
		"      AND t2.priority = ?2"
		"      AND t2.id != task.id"
		") WHERE id = ?1";

	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_set_priority: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
	sqlite3_bind_int(stmt, 2, (int)new_priority);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	RETURN_ERR_IF(rc != SQLITE_DONE,
		"storage_task_set_priority: step failed: %s", sqlite3_errmsg(db));
	return RT_SUCCESS;
}

int storage_task_has_subtasks(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_has_subtasks: storage not open");
	return scalar_count("SELECT COUNT(*) FROM task WHERE parent_id = ?1", id);
}

int storage_task_set_completed(int64_t id, bool completed, bool cascade_subtasks)
{
	RETURN_ERR_IF(db == NULL, "storage_task_set_completed: storage not open");

	/* One statement covers the task and (optionally) its subtasks. Each
	   matched row's manual_order is recomputed via a per-row correlated
	   subquery keyed on that row's own project/parent/priority, so a parent
	   and subtasks of differing priorities each land in their own correct
	   destination group. The "AND status != ?2" guard is required, not just
	   an optimization: SQLite does not guarantee a frozen pre-statement
	   snapshot for correlated subqueries against the same table being
	   written within one UPDATE, so re-touching a subtask already in the
	   target status (e.g. a previously-completed sibling swept up by the
	   parent_id match) could see partially-applied sibling rows and land at
	   an unpredictable position; excluding non-transitioning rows removes
	   the hazard entirely, and is also the semantically correct behavior
	   (nothing changed for that row, so it should not move). If two sibling
	   subtasks of the same priority both transition in the same call, their
	   new manual_order values could still tie; list queries break ties by
	   id, so ordering stays deterministic. */
	static const char *sql =
		"UPDATE task SET "
		"    status = ?2,"
		"    completed_at = CASE WHEN ?2 = 1 THEN ?3 ELSE NULL END,"
		"    manual_order = ("
		"        SELECT COALESCE(MAX(t2.manual_order), 0) + 10 FROM task t2"
		"        WHERE t2.project_id = task.project_id"
		"          AND t2.parent_id IS task.parent_id"
		"          AND t2.archived = task.archived"
		"          AND t2.status = ?2"
		"          AND t2.priority = task.priority"
		"          AND t2.id != task.id"
		"    )"
		"WHERE (id = ?1 OR (?4 = 1 AND parent_id = ?1)) AND status != ?2";

	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_set_completed: prepare failed: %s", sqlite3_errmsg(db));

	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
	sqlite3_bind_int(stmt, 2, completed ? 1 : 0);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
	sqlite3_bind_int(stmt, 4, cascade_subtasks ? 1 : 0);

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	RETURN_ERR_IF(rc != SQLITE_DONE,
		"storage_task_set_completed: step failed: %s", sqlite3_errmsg(db));
	return RT_SUCCESS;
}

int storage_task_reorder_move(int64_t id, int direction)
{
	RETURN_ERR_IF(db == NULL, "storage_task_reorder_move: storage not open");
	RETURN_ERR_IF(direction != 1 && direction != -1,
		"storage_task_reorder_move: invalid direction %d", direction);

	task_t cur;
	RETURN_ERR_IF(storage_task_get(id, &cur) != RT_SUCCESS,
		"storage_task_reorder_move: task %lld not found", (long long)id);

	const char *cmp = direction > 0 ? ">" : "<";
	const char *ord = direction > 0 ? "ASC" : "DESC";
	char sql[512];
	snprintf(sql, sizeof(sql),
		"SELECT id, manual_order FROM task "
		"WHERE project_id = ?1 AND parent_id IS ?2 AND archived = ?3 AND status = ?4 "
		"  AND priority = ?5 AND manual_order %s ?6 "
		"ORDER BY manual_order %s LIMIT 1", cmp, ord);

	sqlite3_stmt *stmt = NULL;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		LERR("storage_task_reorder_move: prepare failed: %s", sqlite3_errmsg(db));
		task_model_free(&cur);
		return RT_ERROR;
	}
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)cur.project_id);
	if (cur.parent_id == 0)
		sqlite3_bind_null(stmt, 2);
	else
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)cur.parent_id);
	sqlite3_bind_int(stmt, 3, cur.archived ? 1 : 0);
	sqlite3_bind_int(stmt, 4, (int)cur.status);
	sqlite3_bind_int(stmt, 5, (int)cur.priority);
	sqlite3_bind_int64(stmt, 6, (sqlite3_int64)cur.manual_order);

	int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		/* No neighbor: at a group boundary/edge. No-op. */
		sqlite3_finalize(stmt);
		task_model_free(&cur);
		return RT_ERROR;
	}
	int64_t neighbor_id = sqlite3_column_int64(stmt, 0);
	long neighbor_order = (long)sqlite3_column_int64(stmt, 1);
	sqlite3_finalize(stmt);
	task_model_free(&cur);

	RETURN_ERR_IF(storage_begin() != RT_SUCCESS, "storage_task_reorder_move: begin failed");

	static const char *upd_sql = "UPDATE task SET manual_order = ?2 WHERE id = ?1";
	sqlite3_stmt *upd = NULL;
	if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, NULL) != SQLITE_OK) {
		LERR("storage_task_reorder_move: prepare update failed: %s", sqlite3_errmsg(db));
		storage_rollback();
		return RT_ERROR;
	}
	sqlite3_bind_int64(upd, 1, (sqlite3_int64)neighbor_id);
	sqlite3_bind_int64(upd, 2, (sqlite3_int64)cur.manual_order);
	rc = sqlite3_step(upd);
	sqlite3_finalize(upd);
	if (rc != SQLITE_DONE) {
		LERR("storage_task_reorder_move: neighbor update failed: %s", sqlite3_errmsg(db));
		storage_rollback();
		return RT_ERROR;
	}

	upd = NULL;
	if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, NULL) != SQLITE_OK) {
		LERR("storage_task_reorder_move: prepare update failed: %s", sqlite3_errmsg(db));
		storage_rollback();
		return RT_ERROR;
	}
	sqlite3_bind_int64(upd, 1, (sqlite3_int64)id);
	sqlite3_bind_int64(upd, 2, (sqlite3_int64)neighbor_order);
	rc = sqlite3_step(upd);
	sqlite3_finalize(upd);
	if (rc != SQLITE_DONE) {
		LERR("storage_task_reorder_move: self update failed: %s", sqlite3_errmsg(db));
		storage_rollback();
		return RT_ERROR;
	}

	return storage_commit();
}

int storage_task_move_project(int64_t id, int64_t dest_project_id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_move_project: storage not open");

	/* The task appends to the end of its own state/priority group in the
	   destination, the same rule storage_task_insert() uses. Subtasks keep
	   their manual_order: it is scoped by parent_id, which does not change. */
	static const char *sql_task =
		"UPDATE task SET project_id = ?2, manual_order = ("
		"    SELECT COALESCE(MAX(t2.manual_order), 0) + 10 FROM task t2"
		"    WHERE t2.project_id = ?2"
		"      AND t2.parent_id IS task.parent_id"
		"      AND t2.archived = task.archived"
		"      AND t2.status = task.status"
		"      AND t2.priority = task.priority"
		") WHERE id = ?1";
	static const char *sql_subtasks =
		"UPDATE task SET project_id = ?2 WHERE parent_id = ?1";
	const char *stmts[] = { sql_task, sql_subtasks };

	RETURN_ERR_IF(storage_begin() != RT_SUCCESS, "storage_task_move_project: begin failed");
	for (size_t i = 0; i < sizeof(stmts) / sizeof(stmts[0]); i++) {
		sqlite3_stmt *stmt = NULL;
		if (sqlite3_prepare_v2(db, stmts[i], -1, &stmt, NULL) != SQLITE_OK) {
			LERR("storage_task_move_project: prepare failed: %s", sqlite3_errmsg(db));
			storage_rollback();
			return RT_ERROR;
		}
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)dest_project_id);
		int rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		if (rc != SQLITE_DONE) {
			LERR("storage_task_move_project: step failed: %s", sqlite3_errmsg(db));
			storage_rollback();
			return RT_ERROR;
		}
	}
	return storage_commit();
}

int storage_task_delete_cascade(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_delete_cascade: storage not open");
	return exec_with_int64("DELETE FROM task WHERE id = ?1", id);
}

int storage_task_clear_notes(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_clear_notes: storage not open");
	return exec_with_int64("UPDATE task SET notes = NULL WHERE id = ?1", id);
}

int storage_task_archive_completed(int64_t project_id, int *out_count, bool apply)
{
	RETURN_ERR_IF(db == NULL, "storage_task_archive_completed: storage not open");

	int count = scalar_count(
		"SELECT COUNT(*) FROM task WHERE project_id = ?1 AND status = 1 AND archived = 0",
		project_id);
	RETURN_ERR_IF(count < 0, "storage_task_archive_completed: count query failed");

	if (out_count != NULL)
		*out_count = count;

	if (apply && count > 0) {
		RETURN_ERR_IF(exec_with_int64(
			"UPDATE task SET archived = 1 WHERE project_id = ?1 AND status = 1 AND archived = 0",
			project_id) != RT_SUCCESS,
			"storage_task_archive_completed: update failed");
	}
	return RT_SUCCESS;
}

int storage_task_restore(int64_t id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_restore: storage not open");
	return exec_with_int64("UPDATE task SET archived = 0 WHERE id = ?1", id);
}

int storage_task_list_completed_between(time_t start, time_t end,
	task_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(db == NULL || out_arr == NULL || out_n == NULL,
		"storage_task_list_completed_between: invalid arguments");

	/* hit: tasks completed in [start, end), archived or not, in any project.
	   block: one row per top-level task that has a hit (itself or a
	   subtask), timed by its own completion if that is a hit, else by its
	   earliest completed subtask. The outer rows are the hits plus each
	   block's top-level task, so a subtask always follows its parent even
	   when the parent was not completed in the period. Projects come in
	   Projects-pane order (storage_project_list()). */
	static const char *sql =
		"WITH hit AS ("
		"    SELECT id, parent_id, completed_at FROM task"
		"    WHERE status = 1 AND completed_at >= ?1 AND completed_at < ?2"
		"), block AS ("
		"    SELECT COALESCE(parent_id, id) AS top,"
		"           COALESCE(MIN(CASE WHEN parent_id IS NULL THEN completed_at END),"
		"                    MIN(completed_at)) AS at"
		"    FROM hit GROUP BY top"
		") "
		"SELECT " TASK_SELECT_COLUMNS " FROM ("
		"    SELECT t.id, t.project_id, t.parent_id, t.title, t.notes, t.status,"
		"           t.priority, t.manual_order, t.archived, t.created_at, t.completed_at,"
		"           p.builtin AS p_builtin, p.archived AS p_archived,"
		"           p.display_name AS p_name, b.at AS block_at, b.top AS block_top"
		"    FROM task t"
		"    JOIN project p ON p.id = t.project_id"
		"    JOIN block b ON b.top = COALESCE(t.parent_id, t.id)"
		"    WHERE t.id IN (SELECT id FROM hit) OR t.id = b.top"
		") "
		"ORDER BY p_builtin DESC, p_archived, p_name, project_id,"
		"         block_at, block_top, (parent_id IS NOT NULL), completed_at, id";

	sqlite3_stmt *stmt = NULL;
	RETURN_ERR_IF(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK,
		"storage_task_list_completed_between: prepare failed: %s", sqlite3_errmsg(db));
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)end);

	int rc = collect_tasks(stmt, out_arr, out_n);
	sqlite3_finalize(stmt);
	return rc;
}

int storage_task_count_archived(int64_t project_id)
{
	RETURN_ERR_IF(db == NULL, "storage_task_count_archived: storage not open");
	return scalar_count("SELECT COUNT(*) FROM task WHERE project_id = ?1 AND archived = 1",
		project_id);
}
