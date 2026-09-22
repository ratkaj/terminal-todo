# Development Requirements

[Project overview](../README.md) · [Functional requirements](requirements.md) · [User interface](ui.md)

These are existing design constraints, quality targets, and collaboration guidelines. The module layout is illustrative, not a description of implemented code. Future C coding specifics and actual code architecture belong in `docs/developer/`.

For terminal implementation work, also read the [ncurses implementation notes](developer/ncurses-ui.md).

## Architecture

Keep UI, domain logic, and persistence separated.

A possible structure:

```text
src/
    main.c      
    project.c
    task.c
    storage.c
    ui.c
    input.c

include/

tests/

CMakeLists.txt
```

This is illustrative rather than mandatory.

### Critical architectural rule

**Business logic must not depend on ncurses.**

Project discovery, task manipulation, validation, sorting and storage should be independently testable.

ncurses should primarily handle:

```text
application state
      ↓
render

keyboard input
      ↓
action
      ↓
domain operation
      ↓
new application state
```

Avoid embedding database queries or business rules directly into rendering code.

## Testing

Testing is a first-class project requirement.

Unit-test at minimum:

* task creation
* task completion, including parent completion cascading to all subtasks after one confirmation
* independent completion and archived flags, with completion never implicitly archiving a task
* Archive Completed affecting only completed, non-archived tasks in the current project and preserving all metadata
* archive confirmation count, cancellation, and no-op behavior when there are no eligible tasks
* independent task/project archive-visibility filters that never mutate stored state
* task restoration preserving completion, priority, notes, and manual order
* regular-project archive/restore preserving every task; permanent built-in projects remaining unarchivable
* contextual `a` and `A` actions and help labels for both Projects and Tasks, including no action on inapplicable selections
* subtask invariants
* project creation
* explicit empty-project persistence and provisional directory-project creation
* no saved project after leaving an unused provisional project or cancelling/failing its first task creation
* atomic persistence of a provisional project with its first task
* project discovery
* parent-directory project resolution
* home-directory startup selects Today; subdirectories retain normal discovery behavior
* task notes and history timestamps
* pane-dependent `i` actions, including equivalence to `n` in Projects and notes editing only with a selected task
* text-entry key handling without triggering navigation shortcuts
* identical Projects/Tasks/Notes navigation order in large and small windows, Tasks-focused startup, and non-wrapping boundaries
* Escape saves notes and returns to navigation with Notes focused; save failure retains editing mode and the edit buffer and reports the error
* Escape cancels new project/task creation without saving records
* task/subtask forms: Enter saves, Escape discards edits without changing stored values, and failed saves retain input
* subtask creation via s only on top-level tasks, with correct parent/project assignment and cancellation
* task-form arrow navigation and forward-cycling Tab with wraparound, skipping read-only fields, cursor movement in Name, and priority selection without intercepting digits in task names or switching main panes
* task-form Enter submission from either field without treating Enter as a priority-selection action
* context-specific Enter behavior for task lists, forms, reorder mode, project selection, and notes editing
* Space completion and absence of an `x` completion binding
* project counts excluding subtasks
* width-dependent pane visibility and non-wrapping replacement behavior
* centered Help overlay opened with `?`, including its close behavior
* aligned hotkey columns across both rows in task, project, prompt, editor, and future windows
* New Project form containing only Project name, editable directory-basename default, cancellation, persistence, and immediate selection
* project display-name renaming without changing canonical directory identity
* pane-dependent deletion, project/task cascades, and notes-only clearing
* clearing Today, This Week, and Inbox preserves each destination and leaves unrelated tasks intact
* atomic deletion and clearing, including completed tasks, archived tasks, and subtasks
* y/n/Y confirmation, cancellation, independent category suppression, and reset on application restart
* directory-free named projects
* Today/This Week membership isolation from other projects
* sorting
* strict state/P1/P2/P3 ordering and persisted manual order within each state/priority group
* priority and completion changes appending to the correct destination group
* reorder state/priority boundaries, single-task groups, and preserving parent/subtask relationships
* order-mode entry with `o`, movement of the `>` row, and Enter to finish
* project renaming with `r` without changing canonical paths
* priority validation, changes, persistence, and independent subtask priorities
* SQLite operations
* malformed/invalid operations
* filesystem edge cases

UI-specific code may have lower direct coverage than domain logic.

Prefer designing UI logic so that as much behavior as possible can be tested without initializing an actual terminal.

## Code Quality

Compile with strict warnings.

At minimum consider:

```text
-Wall
-Wextra
-Wpedantic
-Werror
```

Development/test builds should support:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Valgrind may additionally be used for memory checking.

Code coverage should be measured, particularly for:

```text
task
project
storage
project-membership/query logic
```

Do not optimize for 100% coverage as a vanity metric. Important behavior and failure paths matter more than raw percentage.

## AI-Assisted Development Model

This project is intended to be developed with substantial AI assistance.

The human developer retains responsibility for:

* architecture
* requirements
* interfaces
* UX decisions
* testing strategy
* acceptance criteria
* code review

The AI agent may implement:

* bounded components
* tests
* SQLite integration
* ncurses plumbing
* build-system changes
* refactoring
* documentation
* static-analysis fixes

The AI must not introduce major architectural changes merely because they simplify implementation.

When requirements are ambiguous and the decision materially affects architecture or UX, ask rather than inventing behavior.

Prefer several small, focused commits over large commits containing unrelated changes.
