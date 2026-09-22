# Functional Requirements

[Project overview](../README.md) · [User interface](ui.md) · [Archiving and ordering](archiving_and_ordering.md) · [Development requirements](development.md)

This document defines required behavior and data constraints. Data fields are conceptual; implementation details will be documented separately in `docs/developer/` as code is written.

## Core Concept

Projects may correspond to directories or be named projects without a directory. `Today` and `This Week` are named projects for manually organizing tasks.

Running:

```bash
cd ~/work/atomrpc
todo
```

should immediately open the tasks associated with the `atomrpc` project.

Tasks are stored in a **central database**, not inside project directories. The tool must not create TODO files or other project-local state that could accidentally enter Git repositories.

Suggested database location:

```text
~/.local/share/todo/todo.db
```

Use SQLite unless there is a strong technical reason not to.

## Project Discovery

Each project has:

* unique ID
* display name
* optional canonical root path (directory-backed projects only)
* archived flag

Example:

```text
ID:   12
Name: atomrpc
Path: /home/user/work/atomrpc
```

For directory-backed projects, the canonical path, rather than directory basename, identifies the directory association. Every project has a unique ID, including named projects without a path.

Named projects such as `Today` and `This Week` participate in project switching but not filesystem discovery.

### Home-directory startup

Starting the interactive application with the current directory equal to the user's home directory (`~`) opens `Today`. Compare canonical paths. This special case takes precedence over directory-project discovery and does not create a project for the home directory.

The exception applies only to the home directory itself; its subdirectories follow normal project discovery and provisional-project rules.

### Subdirectory handling

Starting the application inside a project subdirectory must resolve to the project root.

Example:

```text
~/work/atomrpc/src/plugins/
```

must resolve to:

```text
~/work/atomrpc
```

The application should walk upward from the current working directory looking for a registered project root.

A subdirectory must not accidentally become a separate project when its parent is already registered.

### Project creation and persistence

There are two ways to create a project:

* Explicit creation with `i` while the Projects pane is focused: open the one-field New Project form. When the current directory is an unregistered directory context, prefill the name from its basename; otherwise start with an empty name. The field remains editable. On completing creation, save the new empty project and immediately select it. This is intentional creation, so no task is required to persist it. Pressing `Esc` before completion cancels creation without saving a project.
* Directory-based creation: if neither the current directory nor any ancestor is a registered project root, open a provisional project for the current directory. Save it only when its first project task is successfully created.

Launching the application in an unregistered directory must not by itself save an empty project. Leaving without creating a task, cancelling task creation, or failing to save the first task must leave no project record. Save the provisional project and its first task atomically.

The provisional rule applies only to implicit directory-based creation, not explicitly created or previously saved projects. Registered parent-project discovery takes precedence; launching in a subdirectory must not create a separate project.

A saved project's display name may be renamed later without changing its canonical directory path. In the Projects pane, `r` opens the one-field rename form; directory identity remains path-based.

Project selection and switching are specified in the [UI guide](ui.md#project-switching).

## Task Model

A task should contain approximately:

```text
id
project_id
parent_id
title
notes
status
priority
manual_order
archived
created_at
completed_at
```

Exact representation is an architectural decision.

### Status

Initial task states:

```text
OPEN
COMPLETED
```

Do not introduce complex workflows unless required later.

Completion and archiving are independent. The status remains `OPEN` or `COMPLETED`; a separate archived flag records whether the task is archived. Completed tasks remain visible as history until the user explicitly archives completed tasks.

## Subtasks

Subtasks are a core feature.

Only **one level of nesting** is permitted.

Example:

```text
[ ] Implement project discovery
    [x] Walk parent directories
    [ ] Query registered paths
    [ ] Add filesystem-root tests
```

A normal task may contain subtasks.

A subtask MUST NOT contain another subtask.

This invariant must be enforced by the domain/storage layer, not merely by the UI.

Subtasks belong to the same project as their parent.

Create subtasks with `s` on a selected top-level task and edit them with Enter, following the [task and subtask form rules](ui.md#task-and-subtask-forms). Creating another nesting level is unavailable.

Subtasks may have their own:

* status
* priority
* notes, including any informal dates

Completing all subtasks must not automatically complete the parent.

Attempting to complete a parent with unfinished subtasks should require confirmation rather than silently modifying its children.

After confirmation, completing or uncompleting a parent applies the same status change to all of its subtasks. This is one task operation; do not prompt separately for each child.

## Dates

Dates are free-form text in task notes, for example `Review on Sep 24`.

Do not add structured start, planned, or due-date fields, a task-list date column, or automatic date-based filtering and sorting. Keep `created_at` and `completed_at` as history metadata.

Notes are optional task text displayed in the notes pane for the selected task.

## Today and This Week Projects

`Today` and `This Week` are persistent, named projects without directory associations. They use the same project and task model as directory-backed projects.

Tasks belong to these projects through explicit assignment. Do not automatically collect tasks from other projects, derive membership from dates, or reset membership at day/week boundaries.

Selecting either project shows only its own tasks. Cross-project search remains a separate operation.

Project and project-list counts include top-level tasks only. Subtasks do not contribute to counts.

Other useful views may include:

```text
Current Project
All Projects
Priority
Completed
Inbox
```

These should generally be queries over the same underlying task data.

## Priority and task ordering

Store one priority per task: `P1` (highest), `P2` (elevated), or `P3` (default). Do not store separate urgency or importance fields. Validate the three allowed values in the domain/storage layer.

New tasks and subtasks default to `P3` unless a priority is explicitly chosen. Subtasks do not inherit their parent's priority. Editing an existing task preserves its current priority unless changed.

### Task ordering

Order each peer list first by display state (`ACTIVE`, `COMPLETED`, then `ARCHIVED` when archived tasks are displayed), then by priority (`P1`, `P2`, `P3`), then by the user's persisted manual order. The archived flag takes precedence over completion for display grouping. Archived tasks are omitted by default.

Ordering applies among peers: top-level tasks within a project, and subtasks under the same parent. Moving a parent keeps its subtasks attached. Reordering cannot cross a state or priority boundary and must not change a task's completion, archived state, priority, project, or parent.

Changing a task's priority inserts it at the end of the destination state/priority group. Completing or uncompleting a task inserts it at the end of the corresponding destination group. This prevents a manual-order value from a previous group from determining its new position.

Persist manual order across project switches and application restarts. Keep ordering rules in the domain/storage layer so they can be tested independently of ncurses.

See [priority presentation](ui.md#priority-presentation) and [reorder mode](ui.md#reorder-mode) for colors and keyboard interaction.

## Archiving and restoration

Archived tasks and projects remain in the database and are hidden by default. Displaying archived records is a pane-local UI filter and never changes stored state. Use the terms **Display archived** and **Hide archived**; there is no separate archive location or mode.

In the Tasks pane, Archive Completed marks every completed, non-archived task in the current project as archived after confirmation. It must not affect another project's tasks. Archiving preserves completion, priority, notes, ordering metadata, and all other task data. Restoring a selected archived task clears only its archived flag, so a restored completed task returns to its completed state/priority group.

In the Projects pane, a regular project can be archived or restored individually. Archiving or restoring a project changes only the project's archived flag and never changes its tasks. `Today`, `This Week`, and `Inbox` are permanent built-in destinations and cannot be archived.

The precise keys and contextual behavior are defined in [Archive controls](ui.md#archive-controls). Detailed state examples and invariants are in [Archiving and ordering](archiving_and_ordering.md).

## Inbox

Support tasks that are not yet associated with a project.

The Inbox is useful for immediate capture when project classification is unnecessary or unknown.

Inbox should behave as another view rather than requiring a special independent task system.

## Deletion and clearing

Deletion acts on the selection in the focused pane:

* Projects: delete the selected project and all its tasks, including subtasks, completed tasks, and archived tasks. This deletes application data, not the associated filesystem directory.
* Tasks: delete the selected task. Deleting a parent also deletes all its subtasks; deleting a subtask leaves its parent and siblings intact.
* Notes: clear the selected task's notes, leaving the task and its subtasks intact.

`Today`, `This Week`, and `Inbox` are permanent built-in destinations. The project-pane delete action clears all their tasks, including subtasks, completed tasks, and archived tasks, while keeping the destination available. Clearing Inbox affects only tasks belonging to Inbox.

Keep cascading deletion and clearing atomic in the domain/storage layer. Confirmation is one UI operation, without separate prompts for contained tasks or subtasks; see the [deletion interaction](ui.md#deletion).

## Search

Search is future scope and is not part of the initial interface or hotkey set. When implemented, it should provide fast incremental/fuzzy search.

Search should work across projects and should optionally include completed tasks.

Example:

```text
/ mqtt reconnect

atomrpc   [done] Implement MQTT ACL handling
atomrpc   [open] Investigate broker reconnect
panzerpi  [open] Test MQTT recovery
```

Search should also provide a practical way to jump directly to a result.
