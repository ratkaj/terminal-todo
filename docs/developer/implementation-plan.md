# Implementation plan

This is the C implementation plan approved via plan mode on 2026-09-22 (originally
saved only in that session's transcript, under this repository's earlier name
`todo2`). It is preserved here so it isn't lost to session/transcript
rotation. Update the **Status** section as work lands; keep the plan body
below it as the historical record of what was agreed, editing it only to add
new steps or record scope changes, the way the rest of this log-style
documentation works.

## Status (updated 2026-09-22)

Steps 1–16 are implemented and verified: all domain/storage modules
(`task_model`, `storage`, `confirm`, `project_resolve`, `task`, `project`,
`ui_layout`, `ui_state`, `notes_editor` tmpfile helpers, `input_dispatch`)
have passing Unity tests; `ui_draw`/`app_main`'s event loop, the task/project/
new-project forms, the real `$EDITOR` hand-off, Compact/Minimal layouts with
`KEY_RESIZE` handling, archive/delete/reorder UI wiring, the project switcher,
and the help overlay are all wired into the running `todo` binary.

**Step 17 (Polish) completed 2026-09-22:**

* `-Wall -Wextra -Wpedantic -Werror` — clean (already was).
* `has_colors()` color-degradation fallback — present (already was); smoke-
  tested live under `TERM=dumb`/`TERM=vt100`/`TERM=xterm-256color` via a pty
  harness, all exit cleanly.
* Unicode/`wcwidth`-aware truncation — fixed. Added a shared
  `clip_to_cols()` helper in `src/ui_draw.c` (measures by `wcwidth()`
  display columns via `mbrtowc()`, never splits a multi-byte UTF-8 sequence)
  and routed every text-drawing site through it: `put_clipped()`,
  `draw_footer_entry_at()`, the project-list and task-title rows, the new
  `put_clipped_padded()` (confirm/reorder full-width bars, which also
  right-pads to the window width), and `draw_wrapped_text()`'s hard-break
  fallback. Verified visually with a pyte-rendered pty harness: task titles
  containing accents, CJK text, and emoji truncate cleanly at both Wide and
  Compact widths with no corruption, and the confirm bar renders a
  Unicode title correctly. See `docs/developer/ncurses-ui.md`'s "measure and
  truncate by terminal display cells" rule.
* Bottom-right-cell `add_wch` `ERR` case — reviewed: the codebase uses
  `mvwprintw`/`waddch`, not raw `add_wch`, and no code anywhere checks a
  drawing call's return value as fatal, so the known ERR-at-cursor-limit
  case was already harmless by construction. Documented this deliberately
  with a comment at `put_clipped_padded()` (the one place that writes into
  that exact cell, via the single-row `newwin(1, cols, rows-1, 0)` footer/
  confirm/reorder windows) instead of leaving it as an unexplained omission.
* ASan/UBSan test run — run for the first time; the first pass found a real
  leak (`st->provisional_project.canonical_path`, heap-allocated by
  `project_resolve_or_provisional()`/`project.c:65`, was never freed after
  `project_commit_provisional_with_task()` committed it — fixed in
  `src/input_dispatch.c`'s `dispatch_task_form()` with a
  `project_model_free(&st->provisional_project)` call after a successful
  commit). Re-run after the fix: all 11 test binaries pass clean, 0 leaks,
  0 sanitizer errors.
* gcov/lcov coverage review — run for the first time: 87.0% lines / 97.2%
  functions overall. Reviewed `task.c`/`project.c`/`storage.c` specifically
  per the plan's instruction not to chase raw percentage: the uncovered
  lines are (a) `sqlite3_prepare_v2`/`sqlite3_step` failure branches
  throughout `storage.c`, which need DB-connection fault injection to
  reach and aren't worth building infrastructure for at this project's
  scope, and (b) real-filesystem-dependent code (`storage_default_path()`'s
  `$HOME`-based default DB path, `project_resolve_or_provisional()`'s
  provisional-path branch) that the plan already flagged as verified by
  running the real binary rather than by unit test. No genuine untested
  *behavior* under normal operation was found.

**Discovered during step 17's manual verification, out of step 17's scope:**
text-entry fields read input via `wgetch()`, which delivers multi-byte UTF-8
keystrokes one byte at a time and currently drops/mangles non-ASCII
characters typed into a title — a separate, larger fix (wide-character input
throughout `input_dispatch_key()`'s API) than this polish pass covered. Logged
in `docs/agent-lessons.md`'s Known follow-ups.

Also still outstanding, called out in the plan's own Verification section but
not one of the 17 numbered steps: per-module docs under
`docs/developer/modules/` (the directory exists but is empty).

**Step 18 added 2026-09-22, implemented 2026-09-22** (see below).

## Full plan text

The sections below are the plan as approved, unedited except for this note.

---

# C Implementation Plan — ncurses Task Manager

## Context

This repository currently contains only product/UI documentation (`docs/`) plus a
generic C project skeleton (autotools + Unity tests + gcov/lcov coverage) with a
placeholder name `skeleton` and a throwaway `example` module. No task-manager
code exists yet. The goal of this plan is to turn that skeleton into the
ncurses + SQLite personal task manager described in `README.md`,
`docs/requirements.md`, `docs/ui.md`, and `docs/archiving_and_ordering.md`.

The user's stated priorities for this plan:
* the ncurses UI/UX and maintaining UI state are expected to be the hardest
  part;
* strong preference for modular, independently-testable code, built and
  tested module-by-module before final integration;
* **lean on SQLite for what it already solves** — ordering, grouping,
  filtering, indexing, cascading updates — rather than re-implementing that
  logic as C algorithms over in-memory arrays. This was explicit push-back on
  an earlier draft of this plan that added a separate pure-C ordering module;
  the revised architecture below expresses grouping/ordering/append/reorder/
  cascade logic as SQL inside `storage.c` and keeps the C domain layer thin
  (validation + orchestration only). `docs/development.md`'s rule is that
  business logic must not depend on **ncurses** and must be testable
  independent of it — it does not forbid expressing that logic in SQL, and
  "domain/storage layer" in `docs/archiving_and_ordering.md` explicitly
  includes storage.

**Scope decision made in this planning session:** the user does not want the
"Fast CLI Capture" (`todo add ...`) feature — the program is meant to be run
only as the interactive ncurses UI. `docs/requirements.md` currently documents
`todo add [--priority P1] [--today] "title"` as a requirement; the user
confirmed this was a leftover from a previous agent/session and should be
dropped. This plan does **not** implement CLI capture.
**Follow-up for the user:** `docs/requirements.md`'s "Fast CLI Capture"
section should be removed or marked out-of-scope once out of plan mode — that
edit is not made here since plan mode only permits editing this plan file.

This also resolves the two items in `docs/agent-lessons.md`'s "Known
follow-ups": the `--priority P1` CLI syntax question is moot (CLI capture is
dropped entirely), and sibling-only reorder scope is confirmed as already
correctly captured in `docs/requirements.md`/`docs/archiving_and_ordering.md`
(reordering never crosses a state/priority boundary and only ever moves a task
among its peers — top-level siblings, or subtasks of the same parent).

**Third scope decision made in this planning session:** notes editing does not
use an in-app multiline text editor (cursor movement, word-wrap, scrolling) as
`docs/ui.md`'s Notes-editor Enter/Esc rules and
`docs/developer/ncurses-ui.md`'s "Implement notes wrapping and scrolling
explicitly" guidance describe. Building and testing a real text-editing widget
is expensive for little benefit when a real editor already does it better.
Instead, pressing `i` on a selected task's Notes pane shells out to the user's
`$EDITOR` (falling back to `vi` if unset) against a temp file — the same
pattern `git commit`/`crontab -e` use — and reads the saved file back as the
new notes text. See the `notes_editor` module below.
**Follow-up for the user:** `docs/ui.md`'s Notes-pane/Enter-context rules and
`docs/developer/ncurses-ui.md`'s "Notes and focus" section describe an in-app
inline editor and should be rewritten to describe the `$EDITOR` hand-off
instead, once out of plan mode.

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

All new modules follow the existing skeleton convention from `common.h`:
`int fn(...)` returning `RT_SUCCESS`/`RT_ERROR` with `RETURN_ERR_IF` guards (or
`T *fn(...)` with `RETURN_NULLERR_IF`), `LERR`/`LWARN`/`LINFO`/`LDBG` logging,
tabs, C11. `logger.c`/`logger.h`/`common.h` are kept as-is. `example.c/.h` and
`testexample` are deleted once the first real module exists.

### Step 0 — rename

Run `scripts/rename-project.sh todo` first, as its own commit. This rewrites
`skeleton`→`todo`/`SKELETON`→`TODO` across `configure.ac`, `Makefile.am`,
`src/Makefile.am`, `common.h`'s include guard, log-path strings in tests, etc.
`bin_PROGRAMS` becomes `todo`; existing `testlogger`/`testexample` keep working
unchanged — a safe baseline before any real module lands.

### Module list

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
Unchanged from the original draft: pure geometry, no SQL concern. Given
`(rows, cols)`, decide Wide/Compact/Minimal tier and compute pane rectangles
and footer column layout.
```c
typedef enum { LAYOUT_WIDE, LAYOUT_COMPACT, LAYOUT_MINIMAL } layout_tier_t;
typedef struct { int y, x, h, w; } rect_t;
typedef struct { rect_t projects, tasks, notes, footer; bool projects_visible, notes_visible; } layout_geom_t;

layout_tier_t ui_layout_tier(int rows, int cols);
void ui_layout_compute(int rows, int cols, layout_tier_t tier,
                        pane_focus_t single_pane_shown, layout_geom_t *out);

typedef struct { const char *key, *label; } hotkey_entry_t;
int  ui_layout_footer_height(const hotkey_entry_t *entries, size_t n, int width);
void ui_layout_footer_columns(const hotkey_entry_t *entries, size_t n, int width,
                               int *out_col_widths, size_t max_cols,
                               size_t *out_ncols, size_t *out_nrows);
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
    MODE_REORDER, MODE_CONFIRM, MODE_HELP, MODE_PROJECT_SWITCHER
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
    ACTION_NONE, ACTION_QUIT, ACTION_REDRAW, ACTION_EDIT_NOTES, /* ... */
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
void ui_draw_frame(WINDOW *scr, const app_state_t *st, const layout_geom_t *geom,
                    /* read-only project/task/notes data snapshots */);
void ui_draw_shutdown(void);
```
Draws pane borders/headers (shared-border-ownership scheme, reverse-video only
on the focused heading + its 1-space padding, restored before the rest of the
header), task rows with priority coloring (P1 red / P2 yellow / P3 default,
applied to the whole row's foreground text — using each `task_t.state` field
already computed by `storage.c`, never re-derived), forms, notes editor, help
overlay, confirm prompt, project switcher, and the column-aligned footer using
`ui_layout_footer_columns()`. Not Unity-tested; verified manually/visually
against the four window templates.

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
mechanism.

`main.c` becomes a thin wrapper:
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

Notes on decisions baked into this schema (agent judgment calls, not product
decisions — flagged here per the "separate user-approved requirements from
agent proposals" rule, override any of these freely):

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
  mechanism — noted here so a future agent doesn't hand-roll fuzzy matching
  in C either, but FTS5 is **not** part of this plan since Search is
  explicitly future scope.

## Build system changes

**`configure.ac`**
* `AC_INIT([todo], 1.00)` (via the rename script).
* `PKG_CHECK_MODULES([NCURSESW], [ncursesw])` and `PKG_CHECK_MODULES([SQLITE3], [sqlite3])`
  (both confirmed present on this machine: ncursesw 6.5, sqlite3 3.53).
  Ncursesw wide-character functions need `-D_XOPEN_SOURCE=600`; add it
  explicitly to the ncurses CFLAGS if the `.pc` file doesn't already carry it.
* Add `-Wall -Wextra -Wpedantic -Werror` to the shared CFLAGS applied to every
  target, test binaries included.
* Add `--enable-sanitize` mirroring the existing `--enable-coverage` pattern
  (`AC_ARG_ENABLE`/`AM_CONDITIONAL([ENABLE_SANITIZE])`, `CFLAGS +=
  -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, matching
  `LDFLAGS`). Like the existing coverage flag, note it needs a clean rebuild
  and shouldn't be combined with `--enable-coverage` in the same configure run.

**`src/Makefile.am`**
* `todo_SOURCES` lists every module `.c` file directly (domain + storage + UI),
  matching the existing flat `skeleton_SOURCES = main.c logger.c example.c`
  style.
* `todo_CFLAGS`/`todo_LDADD` gain `${SQLITE_CFLAGS}`/`${SQLITE_LIBS}` and
  `${NCURSESW_CFLAGS}`/`${NCURSESW_LIBS}`.
* One new `test<module>` block per new module, following the exact existing
  shape (only the module's own sources + its direct deps, not `main.c`, so
  gcov stays per-module attributed):
  `task_model`, `confirm`, `ui_layout`, `ui_state`, `input_dispatch` (no
  SQLite/ncurses libs needed for these five — this is what proves
  "testable without a terminal" at the build level, not just in prose);
  `project_resolve`, `task`, `project`, `storage` (these four link
  `${SQLITE_CFLAGS}`/`${SQLITE_LIBS}`, since ordering/grouping/filtering now
  live in real SQL exercised against a real embedded SQLite DB); and
  `notes_editor` (links `${NCURSESW_LIBS}` for symbol resolution only — its
  tests call just the tmpfile read/write helpers and never `initscr()`).
  `ui_draw.c`/`app_main.c` are not unit tested (see Verification below).

**Root `Makefile.am`**
* The hardcoded `tests:` target needs one new `@echo`+`@./src/test<module>`
  block per test binary above (10 new ones). This is an existing manual-sync
  point (its own comment already says "keep in sync with bin_PROGRAMS") — kept
  as-is per "don't introduce architectural changes merely to simplify
  implementation," but called out here explicitly as a per-module step to
  remember.

**`docs/developer/modules/`**
One file per new module from `DEVELOPER_DOC_TEMPLATE.md`'s 14 sections,
written alongside that module's implementation milestone (not batched at the
end), so Lifecycle/Ownership/Behavioral-Semantics sections reflect working
code. `storage.md` in particular should document each query's grouping/
ordering/cascade behavior in prose, since that's now where most of the
"business logic" actually lives.

**`.gitignore`**
This directory is not yet a git repository. Once the user runs `git init`, add
a `.gitignore` covering `todo`, every `test*` binary, `*.gcda`/`*.gcno`/`*.gcov`,
`coverage/`, `coverage.info`, and the autotools-generated files already listed
in `maintainer-clean-local` (`Makefile`, `Makefile.in`, `configure`,
`aclocal.m4`, `autom4te.cache/`, `config.log`, `config.status`, `compile`,
`depcomp`, `install-sh`, `missing`).

## UI state machine (mode × focus threading)

```
app_state_t (mode, focus, filters, form buffers, ...)
      |
  ui_draw_frame(scr, state, geom, data-snapshots)     <- render(state)
      |
  wgetch(scr) -> int key                              <- the only raw ncurses input call
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
`HELP`, `PROJECT_SWITCHER`) is a modal overlay relative to a remembered
`focus` — entering one doesn't change `focus`, and `input_dispatch_key()`
checks `mode != MODE_NAVIGATE` before ever consulting pane-navigation logic,
which is what keeps arrow keys "inside" an open form. `MODE_CONFIRM` carries a
small payload (`category` + message + a deferred action token) so one mode
serves delete, archive-completed, etc.; on `y`/`Y` it calls
`confirm_state_apply_answer()` (pure) then invokes the deferred domain call
before popping back to `MODE_NAVIGATE`. `MODE_PROJECT_SWITCHER` re-runs
`storage_project_search(st->switcher_query, ...)` after each keystroke that
changes the query buffer. Notes editing has no mode of its own: `i` on the
Notes pane stays in `MODE_NAVIGATE` and produces `ACTION_EDIT_NOTES`, which
`app_main.c` handles as one synchronous blocking step (write tmpfile, suspend
curses, run `$EDITOR`, resume curses, read tmpfile, save) — see
`notes_editor` above.

## Implementation order

Steps 1–8 are fully Unity-testable without a terminal (storage tests use a
real, fast, embedded SQLite `:memory:`/temp-file DB — no fake/mock layer).
Step 9 onward requires manual/visual verification in a real terminal — this is
flagged explicitly as the expected, scoped point where the architecture stops
being cleanly unit-testable end-to-end.

1. **`task_model`** — struct/enum defs and trivial priority validation. Zero
   dependencies.
2. **`storage`** — schema (including the nesting-invariant trigger, indexes,
   `PRAGMA foreign_keys=ON`), built-in seeding, and every CRUD/ordering/
   append/reorder-move/archive-completed/completion-cascade/search query
   listed above, tested against a temp-file/`:memory:` SQLite DB. This is now
   the largest and most important milestone — it covers most of the
   `docs/development.md` ordering/grouping/archiving checklist directly via
   SQL assertions (query results in the right order, correct manual_order
   values after append/swap, correct row counts after cascades).
3. **`confirm`** — trivial, isolated; needed early since `task.c`/`project.c`
   take a `confirmed` parameter from day one.
4. **`project_resolve`** — cwd walk-up, home-is-Today, provisional detection,
   tested against a `:memory:` `storage_open()` seeded with a few project
   rows at fabricated canonical paths.
5. **`task` + `project`** — validation + orchestration wired to real
   (temp-file) `storage.c`: one-level invariant surfaced as a clean error,
   cascade-confirmation signaling, provisional-atomic-commit transaction,
   archive/restore, rename, delete/clear.
6. **`ui_layout`** — pure geometry/tier/footer-column computation against the
   three template sizes (128×31, compact, 40×12), no terminal.
7. **`ui_state`** — pure mode/focus struct and transition helpers.
8. **`notes_editor`'s tmpfile helpers** — `notes_editor_write_tmpfile()`/
   `notes_editor_read_tmpfile()` only (round-trip tests: normal text, empty
   notes, large text); `notes_editor_edit()`'s curses-suspend/`$EDITOR`
   orchestration is deferred to step 12, since it needs a real terminal.
9. **`input_dispatch`** — key-to-action mapping tested with plain `int` key
   sequences against real domain logic (`:memory:` storage), including the
   project-switcher's `storage_project_search()` call on each filter
   keystroke and `ACTION_EDIT_NOTES` being returned/blocked correctly by
   focus and task selection. By the end of this step, navigation, form field
   logic, reorder stepping, and confirm routing are all verified without ever
   calling `initscr()`.
10. **`ui_draw` skeleton, Wide layout only** — first real ncurses code:
   `setlocale`+`initscr`+`use_default_colors` bring-up, static rendering of
   the Wide template with seeded data, no interactivity beyond quitting.
   Verified manually against `docs/templates/template-fullsize-main-window.md`.
11. **Wire `app_main`'s event loop** for Wide layout: `wgetch` →
    `input_dispatch_key` → `ui_draw_frame`, including selection rendering,
    priority colors, focused-heading reverse video. This is the second
    "modularity breaks down" point — the live loop, `KEY_RESIZE`, and exact
    keystroke-to-pixel behavior are integration concerns validated by running
    the program, not by Unity tests.
12. **Task/Project/New-Project forms, and `notes_editor_edit()`'s `$EDITOR`
    hand-off** — `ui_draw` rendering for
    `template-task-form.md`/`template-new-project.md`, wired to the
    already-tested `input_dispatch` field-navigation logic; and wiring
    `app_main.c`'s `ACTION_EDIT_NOTES` handling to a real
    `def_prog_mode()`/`endwin()`/`system($EDITOR)`/`reset_prog_mode()` cycle.
    Verify manually: normal edit-and-save, empty initial notes, `$EDITOR`
    unset (falls back to `vi`), and a non-zero editor exit (notes left
    unchanged).
13. **Compact and Minimal layouts + `KEY_RESIZE` handling** — extend
    `ui_layout`/`ui_draw` per their templates; resize recompute +
    selection/scroll clamping (clamping itself as a small pure, tested helper).
14. **Archiving/deletion/reorder UI wiring** — connect already-tested
    `task_archive_completed`/`project_archive`/`task_delete`/
    `task_reorder_step` to real confirm-prompt rendering and the `o`-mode
    `>`-marker visuals.
15. **Project switcher (`p`) UI** — incremental-filter rendering on top of the
    already-tested `storage_project_search()`.
16. **Help overlay (`?`)** — centered box; which labels apply to the current
    focus/mode can be a small pure function
    (`help_overlay_entries(app_state_t*, hotkey_entry_t out[], size_t max)`)
    tested without ncurses, with only the box-drawing itself ncurses-specific.
17. **Polish** — color-degradation fallback (test with a mono `TERM`), Unicode
    /`wcwidth` truncation edge cases, the bottom-right-cell `add_wch` `ERR`
    case, a full `-Wall -Wextra -Wpedantic -Werror` cleanup pass, an
    ASan/UBSan test run, and a gcov/lcov review focused on
    task/project/storage paths (not raw percentage).

## Verification

* **Unit tests (steps 1–9, 16's pure helper):** `autoreconf -i &&
  ./configure --enable-tests && make && make tests`, plus a dedicated
  `./configure --enable-tests --enable-sanitize && make && make tests` pass
  before considering the domain/storage layer done.
* **Coverage:** `./configure --enable-tests --enable-coverage && make
  coverage`, reviewed for task/project/storage coverage per
  `docs/development.md` (not chased as a vanity percentage).
* **Manual/visual (steps 10, 11, 12, 13, 17):** run the actual `todo` binary (via
  the `run` skill or directly) in a real terminal at, at minimum, the Wide
  reference size (128×31), a Compact width, and the 40×12 minimum-usability
  target from `docs/ui.md`; resize the terminal live to confirm `KEY_RESIZE`
  recomputes layout and clamps selection/scroll; verify both light and dark
  terminal themes for priority colors and a `TERM` without color support for
  graceful degradation.
* Cross-check each finished module's behavior against the specific
  `docs/development.md` "Testing" checklist bullets it's meant to cover, and
  update `docs/developer/modules/<module>.md` alongside the code rather than
  after the fact.

---

## Step 18 — added 2026-09-22: chain subtask creation from a subtask

**User-reported UX problem:** pressing `s` on a top-level task creates a new
subtask under it, but pressing `s` again to add a *second* subtask does not
work, because after creation focus/selection moves to the newly created
subtask, and `s` is currently only wired to fire when the selected task is
itself top-level.

**Requested behavior:** pressing `s` while the current selection is a
*subtask* should create a new sibling subtask under that subtask's parent
(the same parent the selected subtask already belongs to), not be a no-op.

**Current code, for reference (not yet changed):** `src/input_dispatch.c:223`

```c
} else if (key == 's' && sel != NULL && sel->parent_id == 0) {
    app_state_enter_task_form_new_subtask(st, st->current_project_id, sel->id, sel->title);
```

This only fires when `sel->parent_id == 0` (top-level task selected), passing
`sel->id`/`sel->title` as the new subtask's parent/parent-title. There is also
an existing test pinning the current restriction,
`test_navigate_subtask_creation_only_on_top_level_selection` (referenced in
the step-9 implementation-order note above), which will need to change to
reflect the new intended behavior rather than being treated as a regression.

**Implementation sketch:**
* Branch on `sel->parent_id`:
  * `== 0` (top-level task selected): unchanged — `parent_id = sel->id`,
    `parent_title = sel->title`.
  * `!= 0` (subtask selected): `parent_id = sel->parent_id`, but
    `parent_title` must be the *parent's* title, not `sel->title` — `sel`
    only carries its own fields, so this needs a `storage_task_get(sel->parent_id, &parent)`
    lookup (already exists in `storage.h`) to get `parent.title` before
    calling `app_state_enter_task_form_new_subtask()`. The one-level-nesting
    trigger in the schema already guarantees `sel->parent_id`'s own
    `parent_id` is NULL, so this lookup can't recurse further.
* No `ui_state.c`/`app_state_enter_task_form_new_subtask()` signature change
  needed — it already takes `parent_id`/`parent_title` generically.
* Update/replace `test_navigate_subtask_creation_only_on_top_level_selection`
  in `tests/input_dispatch_tests.c` and add a case asserting `s` on a subtask
  opens the form with the *grandparent* task's title as `parent_title` and
  the same `parent_id` as the originating subtask.
* No schema/storage change needed — this is `input_dispatch.c`-only, using an
  already-existing storage call.

**Implemented 2026-09-22**, following the sketch above exactly:
`dispatch_navigate_tasks()`'s `'s'` handler in `src/input_dispatch.c` now
branches on `sel->parent_id`; on a subtask it calls
`storage_task_get(sel->parent_id, &parent)` to get the real parent's title
before opening the new-subtask form. Replaced the old pinning test
`test_navigate_subtask_creation_only_on_top_level_selection` (which asserted
`s` on a subtask was a no-op) with two tests in
`src/tests/input_dispatch_tests.c`:
`test_navigate_subtask_creation_on_top_level_selection` and
`test_navigate_subtask_creation_on_subtask_selection_chains_under_same_parent`.
Verified with the full Unity suite (24/24 passing), a clean ASan/UBSan run,
and a pty+pyte-rendered manual run: creating "Groceries" → subtask "Milk" →
pressing `s` again while "Milk" is selected correctly adds "Eggs" as a second
subtask of "Groceries", without needing to reselect the parent first.
