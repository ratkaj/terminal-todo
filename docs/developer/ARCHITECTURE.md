# Architecture

[Development requirements](../development.md) · [ncurses implementation notes](ncurses-ui.md)

How the C implementation of the ncurses + SQLite task manager described in
`README.md`/`docs/requirements.md`/`docs/ui.md`/`docs/archiving_and_ordering.md`
is actually put together: the module list, the SQLite schema, and the UI
state machine. Read this before extending the app; update it alongside the
code when the architecture changes, the way the rest of this repo's docs
work.

Two design choices worth knowing up front, since they explain a lot of the
module list below:

* **Lean on SQLite for what it already solves** — ordering, grouping,
  filtering, indexing, cascading updates — rather than re-implementing that
  logic as C algorithms over in-memory arrays. Grouping/ordering/append/
  reorder/cascade logic lives as SQL inside `storage.c`; the C domain layer
  stays thin (validation + orchestration only). `docs/development.md`'s rule
  that business logic must not depend on **ncurses** does not forbid
  expressing that logic in SQL, and "domain/storage layer" in
  `docs/archiving_and_ordering.md` explicitly includes storage.
* **No CLI task capture, no in-app notes editor.** The program is
  interactive-ncurses-only; pressing `i` on a selected task's Notes pane
  shells out to the user's `$EDITOR` (falling back to `vi`) against a temp
  file — the same pattern `git commit`/`crontab -e` use — instead of a
  hand-built multiline text widget. See the `notes_editor` module below.

## Architecture overview

Three layers, kept separate per `docs/development.md`'s critical rule
(business logic must not depend on ncurses):

* **Domain** (`task_model`, `confirm`, `project_resolve`, `task`, `project`)
  — thin C: struct/enum definitions, input validation, and orchestration.
  No ncurses. No hand-rolled sorting/grouping/filtering algorithms — those
  are pushed into `storage.c` as SQL.
* **Storage** (`storage`) — the only module that includes `<sqlite3.h>`.
  This is where the real logic lives: schema (with a trigger enforcing the
  one-level-subtask invariant), CRUD, `ORDER BY`-driven state/priority/
  manual-order sorting, `MAX()`-based append-to-group values,
  neighbor-lookup-based boundary-clamped reorder, single-statement bulk
  UPDATEs for completion cascades and archive-completed, and `LIKE`-based
  project filtering for the project switcher.
* **UI** (`ui_layout`, `ui_state`, `input_dispatch`, `ui_draw`, `app_main`) —
  `ui_layout`/`ui_state`/`input_dispatch` contain zero ncurses calls and are
  Unity-testable by feeding plain `(rows,cols)` or `int` key codes; only
  `ui_draw.c` and `app_main.c` touch `<ncurses.h>`/`wgetch`/`initscr`.

New modules follow the existing convention from `common.h`: `int fn(...)`
returning `RT_SUCCESS`/`RT_ERROR` with `RETURN_ERR_IF` guards (or `T *fn(...)`
with `RETURN_NULLERR_IF`), `LERR`/`LWARN`/`LINFO`/`LDBG` logging, tabs, C11.

## Module list

**`src/task_model.c` / `src/include/task_model.h`** (domain)
Shared plain-data types and trivial validation, no I/O, no sorting logic.
```c
typedef enum { PRIORITY_P1 = 1, PRIORITY_P2 = 2, PRIORITY_P3 = 3 } priority_t;
typedef enum { TASK_STATUS_OPEN = 0, TASK_STATUS_COMPLETED = 1 } task_status_t;
typedef enum { TASK_STATE_ACTIVE = 0, TASK_STATE_COMPLETED = 1, TASK_STATE_ARCHIVED = 2 } task_state_t;

typedef struct {
    int64_t id, project_id, parent_id;   /* parent_id == 0 => top-level */
    char    title[TASK_TITLE_MAX];
    char   *notes;                       /* heap-owned, may be NULL */
    task_status_t status;
    priority_t priority;
    long    manual_order;
    bool    archived;
    task_state_t state;                  /* filled by storage.c from its own
                                             ORDER BY CASE expression — a
                                             single source of truth, not
                                             re-derived independently in C */
    time_t  created_at, completed_at;    /* completed_at == 0 if never completed */
} task_t;

typedef struct {
    int64_t id;
    char    display_name[PROJECT_NAME_MAX];
    char   *canonical_path;              /* heap-owned, NULL = named-without-directory */
    bool    archived;
    bool    builtin;                     /* Today / This Week / Inbox */
} project_t;

int task_model_validate_priority(int v);
```

**`src/confirm.c` / `src/include/confirm.h`** (domain, pure)
Three independent, session-only, non-persisted confirmation-suppression
flags (projects/tasks/notes), reused by delete and archive-completed prompts.
```c
typedef enum { CONFIRM_CAT_PROJECTS, CONFIRM_CAT_TASKS, CONFIRM_CAT_NOTES, CONFIRM_CAT_COUNT } confirm_category_t;
typedef struct { bool suppressed[CONFIRM_CAT_COUNT]; } confirm_state_t;

void confirm_state_init(confirm_state_t *cs);
bool confirm_state_should_prompt(const confirm_state_t *cs, confirm_category_t cat);
void confirm_state_apply_answer(confirm_state_t *cs, confirm_category_t cat,
                                 char answer, bool *out_proceed);
```
Kept decoupled from `task.c`/`project.c`: the UI layer asks
`confirm_state_should_prompt`, shows the prompt if needed, and only then calls
the mutating domain function with `confirmed=true`.

**`src/project_resolve.c` / `src/include/project_resolve.h`** (domain)
The filesystem-walk decision logic — this part genuinely isn't a SQL concern
(SQLite doesn't walk directories), but the "is this ancestor path registered"
check is a single `storage_project_find_by_path()` call per ancestor, so no
separate lookup-abstraction/fake-callback layer is introduced: tests seed a
real `:memory:` `storage_open()` with a few project rows and call this
directly, which is simpler than hand-maintaining a mock.
```c
typedef enum { RESOLVE_HOME_TODAY, RESOLVE_REGISTERED, RESOLVE_PROVISIONAL } resolve_kind_t;

int project_resolve_cwd(const char *cwd, const char *home,
                         resolve_kind_t *out_kind, project_t *out_project,
                         char *out_provisional_path, size_t path_cap);
    /* walks cwd upward to '/', calling storage_project_find_by_path() at
       each ancestor; cwd == canonical home short-circuits to
       RESOLVE_HOME_TODAY before any lookup and never creates a project for
       home; no registered ancestor => RESOLVE_PROVISIONAL with the path
       filled in */
int project_resolve_current(resolve_kind_t *out_kind, project_t *out_project,
                             char *out_provisional_path, size_t path_cap);
    /* getcwd()+realpath()+getenv("HOME")+realpath(), then forwards to
       project_resolve_cwd(); the only unit-untested sliver here, verified by
       running the real binary */
```

**`src/task.c` / `src/include/task.h`** (domain — thin validation + orchestration over `storage.c`)
```c
int task_create(int64_t project_id, int64_t parent_id, const char *title,
                 priority_t priority, task_t *out);
    /* RETURN_ERR_IF empty/too-long title or invalid priority, then
       storage_task_insert(); the one-level-nesting invariant is enforced by
       a SQLite trigger (see schema below) — this function turns the
       trigger's SQLITE_CONSTRAINT_TRIGGER abort into a clean RT_ERROR + LERR
       message instead of re-implementing the parent-of-parent check in C */
int task_update_fields(int64_t id, const char *title, const char *notes);
int task_set_priority(int64_t id, priority_t new_priority);
    /* -> storage_task_set_priority(), which computes the destination
       manual_order in the same UPDATE via a correlated subquery */
int task_set_completed(int64_t id, bool completed, bool confirmed_cascade);
    /* if storage_task_has_subtasks(id) && !confirmed_cascade, returns
       RT_ERROR with a "needs confirmation" signal so the UI can prompt once;
       otherwise -> storage_task_set_completed(), a single UPDATE covering
       the task and (if cascading) all its subtasks atomically */
int task_delete(int64_t id);           /* -> storage_task_delete_cascade() */
int task_clear_notes(int64_t id);      /* -> storage_task_clear_notes() */
int task_reorder_step(int64_t id, int direction);  /* -> storage_task_reorder_move() */
int task_move_to_project(int64_t id, int64_t dest_project_id);
    /* rejects subtasks, archived tasks, the same project, and missing or
       archived destinations; -> storage_task_move_project() */
int task_archive_completed(int64_t project_id, int *out_count, bool confirmed);
    /* -> storage_task_archive_completed(); out_count always computed so the
       caller can show "Archive N completed tasks?" before the user answers */
int task_restore(int64_t id);          /* -> storage_task_restore() */
```

**`src/project.c` / `src/include/project.h`** (domain — thin validation + orchestration over `storage.c`)
```c
int project_create_explicit(const char *display_name, const char *canonical_path, project_t *out);
int project_resolve_or_provisional(project_t *out, bool *out_is_provisional);
    /* wraps project_resolve_current(); RESOLVE_PROVISIONAL fills out
       in-memory only, not yet persisted */
int project_commit_provisional_with_task(project_t *provisional, const char *title,
                                          priority_t prio, task_t *out_task);
    /* the one place a hand-written multi-statement transaction remains in C:
       storage_begin() / INSERT project / INSERT task / storage_commit()
       (or rollback on either failure) — this genuinely spans two independent
       logical inserts with no single-statement SQL equivalent */
int project_rename(int64_t id, const char *new_display_name);
int project_archive(int64_t id);       /* RETURN_ERR_IF project.builtin */
int project_restore(int64_t id);
int project_delete_or_clear(int64_t id);
    /* branches on the builtin flag: storage_project_clear_tasks() for
       Today/This Week/Inbox, storage_project_delete_cascade() otherwise */
```

**`src/export.c` / `src/include/export.h`** (domain — pure text rendering, no ncurses)
```c
int export_project_text(int64_t project_id, bool include_archived, time_t now,
                         char **out_text);
    /* storage_project_get() + task_list_visible_rows() -> heap string in the
       plain-text format of ui.md#export; ordering comes from that list, never
       re-sorted here. `now` is injected so tests are deterministic. */
```

**`src/storage.c` / `src/include/storage.h`** (storage, the only `<sqlite3.h>` include)
This is where grouping/ordering/filtering/cascading actually happens, via SQL:
```c
int  storage_open(const char *db_path);     /* NULL => ~/.local/share/todo/todo.db */
void storage_close(void);
int  storage_begin(void); int storage_commit(void); int storage_rollback(void);

/* projects */
int storage_project_insert(const project_t *p, int64_t *out_id);
int storage_project_update(const project_t *p);
int storage_project_get(int64_t id, project_t *out);
int storage_project_find_by_path(const char *canonical_path, project_t *out); /* RT_ERROR = not found */
int storage_project_list(bool include_archived, project_t **out_arr, size_t *out_n);
    /* ORDER BY builtin DESC, archived, display_name — SQL does the ordering */
int storage_project_search(const char *query, bool include_archived,
                            project_t **out_arr, size_t *out_n);
    /* WHERE display_name LIKE '%'||?||'%' [AND archived=0] ORDER BY
       archived, display_name — this single query *is* the project-switcher's
       incremental filtering; no separate C filtering module */
int storage_project_list_move_targets(int64_t exclude_id, project_t **out_arr, size_t *out_n);
    /* WHERE archived=0 AND id != ? — MODE_TASK_MOVE's destination list */
int storage_project_task_count(int64_t project_id);   /* COUNT(*) top-level only, for pane counts */
int storage_project_delete_cascade(int64_t id);        /* one DELETE; ON DELETE CASCADE removes its tasks */
int storage_project_clear_tasks(int64_t id);           /* DELETE FROM task WHERE project_id=? */

/* tasks: every list function returns rows already ordered and filtered by SQL */
int storage_task_list_top_level(int64_t project_id, bool include_archived,
                                 task_t **out_arr, size_t *out_n);
int storage_task_list_subtasks(int64_t parent_id, bool include_archived,
                                task_t **out_arr, size_t *out_n);
    /* both: SELECT ... , (CASE WHEN archived THEN 2 WHEN status=1 THEN 1
       ELSE 0 END) AS state ... ORDER BY state, priority, manual_order;
       [AND archived=0] when include_archived is false */
int storage_task_get(int64_t id, task_t *out);
int storage_task_insert(int64_t project_id, int64_t parent_id, const char *title,
                         priority_t priority, task_t *out);
    /* manual_order is computed inline via a scalar subquery in the same
       INSERT: (SELECT COALESCE(MAX(manual_order),0)+10 FROM task WHERE
       project_id=? AND parent_id IS ? AND archived=0 AND status=0 AND
       priority=?) — "append to end of the destination group" with no C loop */
int storage_task_update_fields(int64_t id, const char *title, const char *notes);
int storage_task_set_priority(int64_t id, priority_t new_priority);
    /* UPDATE task SET priority=?, manual_order=(append-to-new-group scalar
       subquery, as above but keyed on the new priority) WHERE id=? */
int storage_task_has_subtasks(int64_t id);             /* COUNT(*) WHERE parent_id=id */
int storage_task_set_completed(int64_t id, bool completed, bool cascade_subtasks);
    /* one UPDATE ... WHERE id=? [OR parent_id=? when cascade_subtasks]; each
       matched row's manual_order is recomputed via a *per-row correlated*
       subquery keyed on that row's own project_id/parent_id/priority, so a
       parent and its subtasks (which may each have different priorities)
       each land at the end of their own correct destination group in one
       statement */
int storage_task_reorder_move(int64_t id, int direction);
    /* SELECT the immediately-adjacent peer in the same project/parent/state/
       priority group ordered by manual_order in the move direction, LIMIT 1;
       RT_ERROR (no-op) if none found (boundary/edge); otherwise swap the two
       rows' manual_order values inside one transaction */
int storage_task_move_project(int64_t id, int64_t dest_project_id);
    /* one transaction: the task gets the new project_id and a manual_order at
       the end of its state/priority group there; its subtasks get the new
       project_id and keep their parent-scoped manual_order */
int storage_task_delete_cascade(int64_t id);           /* one DELETE; ON DELETE CASCADE removes subtasks */
int storage_task_clear_notes(int64_t id);
int storage_task_archive_completed(int64_t project_id, int *out_count, bool apply);
    /* out_count always computed via COUNT(*) WHERE project_id=? AND
       status=1 AND archived=0; when apply, a single UPDATE ... SET
       archived=1 WHERE the same predicate */
int storage_task_restore(int64_t id);                  /* UPDATE SET archived=0 WHERE id=? */
```
`storage_open()` creates `~/.local/share/todo/` if missing, executes the
schema (below), seeds the three built-in projects idempotently, and issues
`PRAGMA foreign_keys = ON;` (off by default per SQLite connection) so
`ON DELETE CASCADE` actually removes subtasks/tasks instead of leaving
orphans, which is what lets `storage_project_delete_cascade()` and
`storage_task_delete_cascade()` be single statements instead of hand-written
multi-step deletes. Tests use a temp-file or `:memory:` SQLite DB per test —
real SQLite, not mocked, since the point is to verify actual schema/query
behavior including the ordering and cascade SQL itself.

**`src/ui_layout.c` / `src/include/ui_layout.h`** (UI, pure — no ncurses)
Pure geometry, no SQL concern. Given `(rows, cols)`, decide Wide/Compact/
Minimal tier and compute pane rectangles, footer column layout, and list
scroll offsets.
```c
typedef enum { LAYOUT_WIDE, LAYOUT_COMPACT, LAYOUT_MINIMAL } layout_tier_t;
typedef struct { int y, x, h, w; } rect_t;
typedef struct { rect_t projects, tasks, notes, footer; bool projects_visible, notes_visible; } layout_geom_t;

layout_tier_t ui_layout_tier(int rows, int cols);
void ui_layout_compute(int rows, int cols, layout_tier_t tier,
                        pane_focus_t single_pane_shown, int footer_rows,
                        layout_geom_t *out);
    /* footer gets exactly footer_rows, or 0 when that exceeds 3 or the
       height allows (1 row at >=14 rows, 2 at >=18, 3 at >=20) */

typedef struct { const char *key, *label; } hotkey_entry_t;
int  ui_layout_footer_height(const hotkey_entry_t *entries, size_t n, int width);
void ui_layout_footer_columns(const hotkey_entry_t *entries, size_t n, int width,
                               int *out_col_widths, size_t max_cols,
                               size_t *out_ncols, size_t *out_nrows);
    /* as many columns as fit, filled row by row; footer_height is the
       resulting row count */
int  ui_layout_scroll_offset(int scroll, int sel_line, int total_lines, int visible);
    /* minimal scroll that keeps the selected line visible */
```
Tested with hand-picked `(rows,cols)` pairs matching the three templates
(128×31, the compact width, 40×12) and hotkey-label arrays — no terminal
required.

**`src/ui_state.c` / `src/include/ui_state.h`** (UI, pure — no ncurses)
The application/mode state machine: owns mode, focus, per-pane
archive-visibility filters, selection/scroll indices, form buffers, reorder
marker, confirm-prompt payload, and the project-switcher's filter-text buffer
(the *matching* itself is `storage_project_search()`, not state-machine code).
There is no `MODE_NOTES_EDIT`/`notes_edit_state_t` — with the `$EDITOR`
hand-off (see `notes_editor` below), editing notes is one synchronous,
blocking action dispatched from `MODE_NAVIGATE`, not a mode the app has to
keep live state for between input-loop iterations.
```c
typedef enum {
    MODE_NAVIGATE, MODE_TASK_FORM, MODE_PROJECT_FORM,
    MODE_REORDER, MODE_CONFIRM, MODE_HELP, MODE_PROJECT_SWITCHER,
    MODE_TASK_MOVE
} app_mode_t;
typedef enum { FOCUS_PROJECTS, FOCUS_TASKS, FOCUS_NOTES } pane_focus_t;

typedef struct {
    app_mode_t mode;
    pane_focus_t focus;
    bool archived_shown_projects, archived_shown_tasks;   /* independent per pane */
    confirm_state_t confirm;
    int project_sel, project_scroll, task_sel, task_scroll;
    task_form_state_t task_form;
    project_form_state_t project_form;
    confirm_prompt_t pending_confirm;
    reorder_state_t reorder;
    task_move_state_t task_move;             /* task id + title + highlighted destination */
    int help_scroll;                         /* first visible Help row; ui_draw clamps it */
    char switcher_query[PROJECT_NAME_MAX];   /* text only; results come from storage */
} app_state_t;
```
Small, pure, individually-testable transition helpers, e.g.
`app_state_focus_left(app_state_t*, layout_tier_t)`,
`app_state_enter_task_form_new(...)`, `app_state_confirm_answer(app_state_t*, char)`.
These mutate `app_state_t` only and never call `storage.c` directly.

**`src/notes_editor.c` / `src/include/notes_editor.h`** (UI, mostly ncurses-adjacent)
Shells out to `$EDITOR` for notes editing instead of an in-app text widget.
Split so the genuinely pure part is separately testable:
```c
int notes_editor_write_tmpfile(const char *text, char *out_path, size_t path_cap);
    /* mkstemp() + write text (or an empty file if text is NULL); pure file
       I/O, no ncurses — directly unit-testable */
int notes_editor_read_tmpfile(const char *path, char **out_text);
    /* reads the whole file into a heap buffer; pure file I/O, directly
       unit-testable */
int notes_editor_edit(const char *initial_text, char **out_text);
    /* orchestration, NOT unit-tested (needs a real terminal + a real editor
       process): notes_editor_write_tmpfile() -> def_prog_mode()+endwin() to
       suspend curses -> build "<$EDITOR or vi> <tmpfile>" and run it via
       system() (a shell is wanted here so an $EDITOR value containing flags,
       e.g. "code --wait", is parsed correctly) -> reset_prog_mode()+
       doupdate() to resume curses -> notes_editor_read_tmpfile() -> unlink()
       the tmpfile -> RT_SUCCESS, or RT_ERROR if the editor exited non-zero
       (treated as "cancelled, keep existing notes", mirroring how `git
       commit` discards an aborted message) */
int notes_editor_view(const char *text, const char *name_hint);
    /* read-only variant for export: mkstemps() a
       todo_export_<name_hint>_XXXXXX.txt tmpfile, run the same editor
       hand-off, ignore the exit status, always unlink() */
```
`notes_editor_edit()` references ncurses symbols (`def_prog_mode`/`endwin`/
`reset_prog_mode`), so its test binary must still link `ncursesw` for the
symbol to resolve, but the tests only ever call the two tmpfile helpers —
`initscr()` is never called under Unity, so no real terminal is required.

**`src/input_dispatch.c` / `src/include/input_dispatch.h`** (UI, pure — no ncurses)
Translates a raw key (plain `int`, matching `wgetch()`'s return including
`KEY_*` constants) plus the current `app_state_t` into a domain action,
executes it via `task.c`/`project.c`/`storage_project_search()`, and updates
`app_state_t`. Testable by feeding sequences of plain `int` key codes against
a real `:memory:`-backed domain layer, with zero ncurses calls.
```c
typedef enum {
    ACTION_NONE, ACTION_QUIT, ACTION_REDRAW, ACTION_EDIT_NOTES,
    ACTION_COPY_NOTES, ACTION_EXPORT,
} dispatch_result_t;
dispatch_result_t input_dispatch_key(int key, app_state_t *st, layout_tier_t tier);
```
Branches first on `st->mode`: `MODE_NAVIGATE` handles pane/hotkey routing by
`focus`; every other mode is a modal overlay that intercepts all input for its
own concern without changing `st->focus`. This is the concrete mechanism
behind "Left/Right moves the cursor while the form is open ... does not
navigate main panes" — the pane-navigation branch is simply unreachable while
`mode != MODE_NAVIGATE`. `i` while `focus == FOCUS_NOTES` and a task is
selected returns `ACTION_EDIT_NOTES` (still `RT_ERROR`/no-op with no task
selected, per the existing rule) without changing `st->mode` at all — the
actual editor hand-off is a synchronous action performed by `app_main.c`
(see below), not a dispatch-level state transition.

**`src/ui_draw.c` / `src/include/ui_draw.h`** (UI, ncurses — the only module allowed `<ncurses.h>`/`<locale.h>`)
```c
int  ui_draw_init(void);     /* setlocale, initscr, cbreak/noecho/keypad,
                                 start_color + use_default_colors with
                                 graceful degrade if colors unsupported */
void ui_draw_frame(app_state_t *st);
    /* builds the focused pane's footer entries, sizes the footer from
       ui_layout_footer_height(), and writes back only task_scroll and
       project_scroll, which depend on pane heights known here */
void ui_draw_shutdown(void);
```
Draws pane borders/headers (shared-border-ownership scheme, reverse-video only
on the focused heading + its 1-space padding, restored before the rest of the
header), task rows with priority coloring (P1 red / P2 yellow / P3 default,
applied to the whole row's foreground text — using each `task_t.state` field
already computed by `storage.c`, never re-derived), forms, notes editor, help
overlay, confirm prompt, project switcher, and the column-aligned footer using
`ui_layout_footer_columns()`. Not Unity-tested; verified manually/visually
against the window templates in `docs/templates/`. Truncates and word-wraps text by terminal
display column via `clip_to_cols()` (a `wcwidth()`-based helper), not by byte
count, so multi-byte UTF-8 titles/notes render correctly.

**`src/app_main.c` / `src/include/app_main.h`** (UI, ncurses event loop glue)
```c
int app_main_run(void);
```
`storage_open()` → `project_resolve_or_provisional()` → build initial
`app_state_t` → loop: `wgetch()` → `input_dispatch_key()` → re-fetch data
snapshots from `task.c`/`project.c`/`storage_project_search()` →
`ui_draw_frame()`; handles `KEY_RESIZE` by recomputing `ui_layout_compute()`
and clamping selection/scroll. `wgetch()` is the only raw ncurses input call
in the whole loop, with one deliberate exception: when `input_dispatch_key()`
returns `ACTION_EDIT_NOTES`, `app_main.c` calls `notes_editor_edit()`
synchronously (blocking the loop while the external editor runs), then
`task_update_fields()` with the returned text. If that save fails,
`app_main.c` re-invokes `notes_editor_edit()` with the same text so nothing
is lost and surfaces the error on the next frame — approximating the original
"keep the edit buffer and show the error" rule despite the different
mechanism. `ACTION_EXPORT` (`e` in Tasks) is handled the same way:
`export_project_text()` for the current project and archive filter, then
`notes_editor_view()`.

`main.c` is a thin wrapper:
```c
int main(void) {
    logger_init(...);
    int rc = app_main_run();
    logger_close();
    return rc == RT_SUCCESS ? 0 : 1;
}
```

## Data model / SQLite schema

```sql
CREATE TABLE IF NOT EXISTS project (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    display_name   TEXT NOT NULL,
    canonical_path TEXT UNIQUE,               -- NULL = named-without-directory
    archived       INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0,1)),
    builtin        INTEGER NOT NULL DEFAULT 0 CHECK (builtin IN (0,1))
);

CREATE TABLE IF NOT EXISTS task (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id    INTEGER NOT NULL REFERENCES project(id) ON DELETE CASCADE,
    parent_id     INTEGER REFERENCES task(id) ON DELETE CASCADE,  -- NULL = top-level
    title         TEXT NOT NULL,
    notes         TEXT,
    status        INTEGER NOT NULL DEFAULT 0 CHECK (status IN (0,1)),
    priority      INTEGER NOT NULL DEFAULT 3 CHECK (priority IN (1,2,3)),
    manual_order  INTEGER NOT NULL DEFAULT 0,
    archived      INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0,1)),
    created_at    INTEGER NOT NULL,     -- unix epoch seconds
    completed_at  INTEGER               -- NULL until first completed
);

CREATE INDEX IF NOT EXISTS idx_task_project_toplevel ON task(project_id, parent_id);
CREATE INDEX IF NOT EXISTS idx_task_parent ON task(parent_id);

-- One-level-nesting invariant enforced by SQLite itself, not by C validation:
CREATE TRIGGER IF NOT EXISTS task_no_double_nesting
BEFORE INSERT ON task
WHEN NEW.parent_id IS NOT NULL
BEGIN
    SELECT RAISE(ABORT, 'subtasks cannot themselves have subtasks')
    WHERE (SELECT parent_id FROM task WHERE id = NEW.parent_id) IS NOT NULL;
END;
```

Notes on decisions baked into this schema (implementation judgment calls, not
product decisions — override any of these freely if a real need shows up):

* **One-level nesting is a database trigger**, not a C `if` check copied into
  every insert path. This is the strongest, most bulletproof form of "enforced
  by the domain/storage layer, not merely by the UI" — it's impossible to
  violate regardless of which code path inserts a row. `task_create()` still
  turns the trigger's abort into a clean `RT_ERROR` + log message for the UI,
  but the actual guarantee lives in the schema.
* `parent_id NULL` (not a `0`/self-reference sentinel) for top-level tasks —
  idiomatic SQLite, works cleanly with `IS NULL` queries, `ON DELETE CASCADE`,
  and the trigger above.
* `Today`/`This Week`/`Inbox` are plain `project` rows with `builtin=1`,
  `canonical_path=NULL`, seeded idempotently (`INSERT ... WHERE NOT EXISTS`,
  matched by `display_name`) on every `storage_open()` — no separate
  migration/schema-version table for this v1 schema. `builtin=1` is what
  `project_archive()`/`project_delete_or_clear()` check.
* `manual_order` spaced by 10, computed by a scalar subquery
  (`MAX(manual_order)+10` within the destination group, or `10` if the group
  is empty) inline in the same `INSERT`/`UPDATE` — no C-side loop or
  renumbering pass, and none is needed for v1: swaps only ever exchange two
  existing values, and priority/completion changes always *append* rather
  than insert between two adjacent values.
* `status`/`archived` as small `INTEGER` 0/1, consistent with the existing
  `RT_SUCCESS`/`RT_ERROR` small-integer-code convention already in the
  codebase.
* `PRAGMA foreign_keys = ON` at every `storage_open()` — SQLite defaults this
  off per-connection; enabling it is what makes single-statement
  `DELETE FROM project WHERE id=?` / `DELETE FROM task WHERE id=?` correctly
  cascade to subtasks/tasks instead of requiring hand-written multi-step
  deletes in C.
* Project-switcher filtering (`docs/ui.md`'s "incremental filtering" — note
  fuzzy matching is explicitly reserved for the future, out-of-scope Search
  feature) is a single `LIKE '%query%'` query, not a C string-matching module.
  If full-text/fuzzy search is implemented later per `docs/requirements.md`'s
  Search section, SQLite's FTS5 virtual-table extension is the natural
  mechanism — noted here so a future contributor doesn't hand-roll fuzzy
  matching in C either, but FTS5 is **not** implemented since Search is
  future scope.

## Adding a new module

Follow the existing shape in `src/Makefile.am`:
* List the module's `.c` file in `todo_SOURCES`.
* Add a `test<module>` block: only the module's own sources plus its direct
  deps (not `main.c`, so gcov stays per-module attributed). Pure domain/UI
  modules (`task_model`, `confirm`, `ui_layout`, `ui_state`,
  `input_dispatch`) need no SQLite/ncurses libs — that's what proves
  "testable without a terminal" at the build level. Modules touching storage
  link `${SQLITE_CFLAGS}`/`${SQLITE_LIBS}`; modules touching ncurses symbols
  (even only for linking, like `notes_editor`) link `${NCURSESW_LIBS}`.
* Add the matching `@echo`+`@./src/test<module>` block to the root
  `Makefile.am`'s `tests:` target — its own comment says "keep in sync with
  bin_PROGRAMS," and it's a manual step, easy to forget.
* Optionally add `docs/developer/modules/<module>.md` from
  `DEVELOPER_DOC_TEMPLATE.md`, written alongside the implementation so it
  reflects working code rather than being batched on at the end.

## UI state machine (mode × focus threading)

```
app_state_t (mode, focus, filters, form buffers, ...)
      |
  ui_draw_frame(&state)                               <- render(state)
      |
  wgetch(stdscr) -> int key                           <- the only raw ncurses input call
      |
  input_dispatch_key(key, &state, tier)               <- action decision + domain call
      |   (may call task_create / task_set_completed / project_archive /
      |    storage_project_search / ...)
  updated app_state_t + mutated storage
      |
  re-fetch task/project/notes snapshots (already ordered/filtered by SQL)
      |
  loop
```

`MODE_NAVIGATE` is the only mode where `focus` drives Left/Right/hotkey
routing. Every other mode (`TASK_FORM`, `PROJECT_FORM`, `REORDER`, `CONFIRM`,
`HELP`, `PROJECT_SWITCHER`, `TASK_MOVE`) is a modal overlay relative to a remembered
`focus` — entering one doesn't change `focus`, and `input_dispatch_key()`
checks `mode != MODE_NAVIGATE` before ever consulting pane-navigation logic,
which is what keeps arrow keys "inside" an open form. `MODE_CONFIRM` carries a
small payload (`category` + message + a deferred action token) so one mode
serves delete, archive-completed, etc.; on `y`/`Y` it calls
`confirm_state_apply_answer()` (pure) then invokes the deferred domain call
before popping back to `MODE_NAVIGATE`. `MODE_PROJECT_SWITCHER` re-runs
`storage_project_search(st->switcher_query, ...)` after each keystroke that
changes the query buffer. `MODE_TASK_MOVE` lists
`storage_project_list_move_targets()` and calls `task_move_to_project()` on
Enter. `MODE_HELP` only tracks a scroll offset; `ui_draw` clamps it to the
drawn height. Notes editing has no mode of its own: `i` on the
Notes pane stays in `MODE_NAVIGATE` and produces `ACTION_EDIT_NOTES`, which
`app_main.c` handles as one synchronous blocking step (write tmpfile, suspend
curses, run `$EDITOR`, resume curses, read tmpfile, save) — see
`notes_editor` above.
