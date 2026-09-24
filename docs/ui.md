# User Interface

[Project overview](../README.md) · [Functional requirements](requirements.md) · [Archiving and ordering](archiving_and_ordering.md) · [Full-size main-window template](templates/template-fullsize-main-window.md) · [New-project template](templates/template-new-project.md)

This document defines terminal interactions and visual behavior. Window layouts live in `docs/templates/`; additional windows will receive separate templates.

## Interaction principles

The ncurses interface is one of the most important parts of the project.

Optimize for:

* keyboard operation
* low interaction latency
* minimal keystrokes
* readability
* predictable navigation
* changing terminal dimensions
* effective use in small Sway/tiling-WM windows

Avoid mouse-dependent functionality.

### Keyboard operation

Pane navigation uses the same Left/Right order at every window size: `Projects ↔ Tasks ↔ Notes`. In large windows, arrows move focus between visible panes. In small windows, they replace the visible pane. Left at Projects and Right at Notes do nothing; navigation does not wrap.

Start with Tasks focused for the resolved project, including Today when launched from `~`. Within the focused task or project pane, Up/Down arrows move between entries.

Initial accepted bindings:

```text
Left/Right  move between panes
Up/Down     navigate tasks/projects
Enter       open selected task/subtask for editing; submit context-specific forms/actions
i           insert/edit according to the focused pane (see below)
n           edit notes for the selected task (Tasks/Notes)
s           create a subtask under the selected parent task
Space       complete/uncomplete selected task
c           copy the selected task's notes to the system clipboard (Notes pane only)
e           export the current project's tasks and notes as plain text (Tasks pane only)
m           move the selected top-level task to another project (Tasks pane only)
d           delete/clear according to the focused pane
a           archive/restore according to the focused pane and selection
A           Show archived/Hide archived in the focused project or task pane
p           focus/open projects pane
1 / 2 / 3   assign P1 / P2 / P3 directly
o           reorder selected task; Up/Down moves `>`; Enter finishes
r           rename selected project (Projects); open selected task/subtask for editing (Tasks)
Esc         cancel task/project forms (does not apply to notes editing; see below)
?           help
q           quit
```

Exact bindings may evolve based on usability.

The `i`, `n`, `s`, `d`, `a`, `A`, `p`, `1`/`2`/`3`, `o`, `r`, `Space`, `c`, `e`, `m`, `Esc`, and arrow-key bindings are accepted. In reorder mode, Up/Down moves the task marked with `>` and Enter finishes reordering instead of opening it. Navigation shortcuts must not intercept normal characters in text-entry fields.

Notes editing does not use an in-app text widget: it hands off to the user's `$EDITOR` (falling back to `vi`) against a temporary file, the same pattern `git commit` uses. The event loop blocks while the editor runs; on return, a zero exit saves the edited text and a non-zero exit discards it, leaving the existing notes unchanged.

Enter has one meaning per active context:

| Context | Enter action |
| --- | --- |
| Tasks pane | Open the selected task or subtask for editing. |
| Notes pane | Same as `i`: hand off to `$EDITOR` for the selected task's notes. |
| Task or project form | Submit: save an edit or create the new item. |
| Reorder mode | Finish and persist the current order. |
| Project selector | Select the highlighted project and return to Tasks. |

The `i` action depends on the focused pane:

| Focused pane | Action | Hotkey label |
| --- | --- | --- |
| Projects | Create a new empty project. | `i New project` |
| Tasks | Create a new task in the current project. | `i Insert` |
| Notes | Hand off to `$EDITOR` for the selected task's notes. | `i Edit notes` |

`n` is a shortcut to the same notes hand-off from the Tasks pane (or Notes), so a task's notes can be opened without first moving focus to Notes. It is not a Projects-pane shortcut; `i` covers project creation there. `c` on the Notes pane copies the selected task's notes to the system clipboard via an OSC 52 terminal escape sequence (no external clipboard tool required, but the terminal must support OSC 52).

Notes editing requires a selected task; it is unavailable when no task is selected.

During new-project or new-task creation, `Esc` cancels creation, discards the unfinished input, and returns to navigation without creating a record. Cancelling the first task in a provisional directory project must not persist that project. Show contextual help such as `Esc Cancel` for the active mode.

The hotkey pane below the main content may span two lines when the shortcuts do not fit comfortably on one. Align hotkey entries in columns across both rows, using consistent column widths sized for the longest entry in each column. Use `1/2/3 Priority` and `p Projects` as the displayed labels; reserve enough layout height for both lines when needed.

This alignment rule applies to every window and dialog, including task forms, the New Project form, prompts, editors, and future templates. If one row has fewer actions, leave the corresponding column empty rather than shifting later entries left.

Primary operations should normally require one key or a very short sequence.

## Task and subtask forms

Use the same [task form](templates/template-task-form.md) for creation and editing. It contains Name (the task's title) and Priority. Notes remain editable in the Notes pane; subtasks remain managed in the main task list.

* `Enter` on an existing task or subtask opens the form with its current values.
* `i` in Tasks opens a New task form for the current project.
* `s` in Tasks opens a New subtask form under the selected top-level task. Show its parent as read-only context. This action is unavailable when a subtask or no task is selected.
* Up/Down moves between the editable Name and Priority fields, skipping the read-only Parent line. `Tab` cycles forward through these fields, wrapping from Priority back to Name.
* Left/Right moves the text cursor when Name is focused, or changes the selected P1/P2/P3 value when Priority is focused. These keys do not navigate the main panes while the form is open.
* `1`/`2`/`3` also selects priority when Priority is focused; digits entered in Name remain text.
* `Enter` always submits the form: it saves an edit or creates the new item, regardless of which field is focused. It does not select a priority. On save failure, keep the form open with the input intact and show the error.
* `Esc` cancels the form and discards its unsaved input. Editing an existing task leaves its saved values unchanged; cancelling creation creates no record.

The task form's `Esc Cancel` has no Notes-pane equivalent: notes editing is a blocking hand-off to `$EDITOR`, not a mode with its own key handling (see [Keyboard operation](#keyboard-operation)).

## Project Switching

The current directory determines the **initial project only**. Starting in `~` opens `Today`, as specified by the [home-directory startup rule](requirements.md#home-directory-startup).

Once the application is running, the user must be able to switch projects without changing the process working directory.

Provide keyboard-driven project selection with incremental filtering, accessed with `p`.

In large windows, `p` focuses the visible projects pane. In small windows, `p` opens the projects pane in place of the tasks pane.

Example:

```text
Switch project
> ato

  atomrpc             4 open
  atomrpc-test        2 open
```

Project switching must be fast enough to become part of normal navigation.

Project counts in the Projects pane count top-level tasks only; subtasks are excluded. The same rule applies to counts for Today, This Week, Inbox, and other projects.

Press `i` in the Projects pane to create a new empty project (`n` is not a shortcut here; it is reserved for quick notes editing from Tasks/Notes, see below). Explicit creation and provisional directory-based creation follow the [project persistence rules](requirements.md#project-creation-and-persistence).

Built-in projects (`Today`, `This Week`, `Inbox`) are always listed first, followed by one blank row, then user-created projects. When archived projects are displayed, a second blank row separates active user-created projects from archived ones. A directory that resolves to a not-yet-saved provisional project is listed ahead of everything else as `[name]`; it becomes a normal, selectable `name` row once its first task is created. Up/Down in the Projects pane immediately updates the Tasks pane to preview the highlighted project's tasks, including the provisional row; Enter simply moves focus to Tasks rather than being required to load it.

When any projects are archived, the Projects pane shows an `Archived: N` count at the bottom of the pane, below the list.

The New Project form contains only one editable field: Project name. When the current directory is an unregistered directory context, it is prefilled with that directory's basename, such as `atomrpc` for `~/work/atomrpc`; the user may replace that default before saving. When creating a named project from an existing project, it starts with an empty name field. Enter saves the project and immediately selects it in the Projects pane. Esc cancels without creating it.

Existing project display names may be renamed while keeping their canonical directory path unchanged. Press `r` in the Projects pane to open the one-field project form with the current name. Enter saves the new name; Esc cancels. The canonical directory path is read-only. In the Tasks pane, `r` is instead an alias for Enter's open/edit action on the selected task/subtask (there is no separate rename-only form for tasks); `r` has no action in Notes.

Archived projects are excluded from project selection and incremental filtering unless archived projects are being displayed. Restoring an archived project makes it part of the normal project list again without changing any of its tasks.

## Archive controls

Archive visibility is maintained separately for the Projects and Tasks panes. It is a UI filter, not an application mode, and it does not modify stored records. The footer and Help overlay use **Show archived** when archived records are hidden and **Hide archived** when they are shown.

In Projects:

| Context | `a` | `A` |
| --- | --- | --- |
| Archived projects hidden; regular project selected | Archive the selected project. | Show archived projects. |
| Archived projects displayed; archived project selected | Restore the selected project. | Hide archived projects. |
| No applicable project selected | No action. | Toggle archive visibility. |

`Today`, `This Week`, and `Inbox` cannot be archived. Archiving or restoring a regular project does not change any of its tasks.

In Tasks:

| Context | `a` | `A` |
| --- | --- | --- |
| Archived tasks hidden | Confirm and archive every completed, non-archived task in the current project. | Show archived tasks. |
| Archived tasks displayed; archived task selected | Restore the selected task. | Hide archived tasks. |
| Archived tasks displayed; active/completed task or no task selected | No action. | Hide archived tasks. |

Before Archive Completed, prompt `Archive N completed tasks? [y/N]`, using the actual number of affected tasks. Only `y` or `Y` confirms; any other response cancels. Do not apply the deletion prompt's session suppression to archiving. If there are no completed, non-archived tasks, do not open the prompt and leave the data unchanged.

Restoring a task clears only its archived flag. Its completion, priority, notes, manual order, and other metadata remain unchanged. Update the contextual `a` label as the focus, visibility filter, or selection changes: `a Archive project`, `a Archive done`, or `a Restore`. There is no archive action while Notes is focused.


## Deletion

In navigation mode, `d` acts on the focused pane using the [deletion and clearing rules](requirements.md#deletion-and-clearing): delete a project and all its tasks (including archived tasks), delete a task and its subtasks, or clear a task's notes. In Projects, selecting `Today`, `This Week`, or `Inbox` clears all its tasks, including archived tasks, instead of removing the destination. If there is no applicable selection, there is no delete action.

Pressing Space on a parent task with subtasks requests one confirmation for the parent and all of its subtasks, then applies the same completion change to every child. Do not prompt separately for individual subtasks. A parent without subtasks toggles immediately.

Show a single confirmation with a short message naming the target and describing what will be deleted or cleared, followed by `y/n/Y`. Mention contained tasks or subtasks when the action removes them. Examples:

```text
Delete project "atomrpc" and all its tasks? y/n/Y
Delete task "Improve ncurses UI" and its subtasks? y/n/Y
Delete task "Write documentation"? y/n/Y
Clear notes for "Implement project discovery"? y/n/Y
Delete all tasks in Today (keep project)? y/n/Y
Delete all tasks in This Week (keep project)? y/n/Y
Delete all tasks in Inbox (keep Inbox)? y/n/Y
```

Keep the message concise; wrap it when necessary without hiding the action or response keys. This remains one prompt, not an additional confirmation.

| Key | Effect |
| --- | --- |
| `y` | Perform this deletion or clearing operation. |
| `n` | Cancel without changing data. |
| `Y` | Perform this operation and suppress further confirmations for the same action category during this application session. |

Maintain three independent confirmation preferences: projects, tasks, and notes. Clearing a built-in destination belongs to the projects category; deleting a parent and its subtasks is one tasks-category operation. Suppression never carries between categories and resets when the application restarts. Do not persist it as configuration.

Do not add separate confirmations for cascading deletion. During notes editing, `d` is ordinary text.

## Export

Press `e` in the Tasks pane to export the current project as plain text. The export opens read-only in `$EDITOR` (falling back to `vi`), using the same blocking hand-off as notes editing, on a temporary file named `todo_export_<project>_XXXXXX.txt`. Save a copy, print, or copy from the editor; the temporary file is deleted when the editor exits, whatever its exit status. The application itself never writes an export file, so nothing lands in project directories.

The export covers what the Tasks pane shows: the current project in display order, with archived tasks included only while archived tasks are displayed. It works with no task selected. It is unavailable for a provisional (unsaved) project.

```text
atomrpc
Path:     /home/user/work/atomrpc
Exported: 2026-09-23 14:05
Tasks:    5 open, 1 completed

[ ] P1  Implement project discovery
        | Walk up from current directory to find
        | the nearest registered project root.
        | Review on Sep 24
    [ ] P3  Query registered paths
    [x] P3  Walk parent directories

[ ] P2  Improve ncurses UI
    [ ] P3  Handle window resize

[ ] P3  Write initial test suite
[ ] P3  Set up CI build

[x] P3  Research SQLite schema
        (completed 2026-09-20)
```

* Header: project name, `Path:` for directory-backed projects only, export time, and top-level counts (subtasks excluded; `, N archived` is appended while archived tasks are displayed).
* Each row: `[ ]`/`[x]`, priority, title; archived rows end with `(archived)`. Subtasks are indented four spaces under their parent.
* Notes are printed verbatim, line by line, behind a `| ` gutter aligned under the title; they are not re-wrapped, and trailing blank lines are dropped.
* Completed top-level tasks get a `(completed YYYY-MM-DD)` line.
* A blank line separates a top-level task from its neighbours when either has notes, subtasks, or a completion date; runs of bare tasks stay compact.
* An empty project prints `(no tasks)` after the header.

## Move task

Press `m` on a top-level task in the Tasks pane to open the [Move task popup](templates/template-task-move.md), which lists non-archived projects other than the current one in Projects-pane order. Up/Down selects a project, Enter moves the task there, and Esc cancels. `m` does nothing on a subtask or an archived task.

The task moves with its subtasks and notes and keeps its priority and completion state; see [Moving tasks between projects](requirements.md#moving-tasks-between-projects). Focus stays on the current project, and the selection moves to a remaining row when the moved task was last.

## Priority presentation

| Priority | Level | Foreground color |
| --- | --- | --- |
| P1 | Highest | Red |
| P2 | Elevated | Yellow |
| P3 | Default | Terminal default (no added color) |

Apply the priority color to the whole task's foreground text, including its checkbox, title, and priority label. Keep the row background unchanged; do not restrict the color to the checkbox brackets. Use the terminal's red, yellow, and default colors, matching the accepted foreground-text preview.

Keep explicit `P1`/`P2`/`P3` labels so priority is readable without color. Subtasks have their own priority and use the same color rules independently of their parent.

Priority values and sorting rules are defined in [functional requirements](requirements.md#priority-and-task-ordering).

## Reorder mode

Press `o` on the selected task to enter reorder mode:

* Up/Down moves the task marked with `>` one position within its current state/priority group, keeping it marked.
* At either end of the group, further movement in that direction does nothing; do not wrap or cross into another state or priority group.
* Enter saves the order and returns to normal navigation.

Use the existing `>` marker to show the task being moved. Contextual help may show `ORDER — Up/Down Move · Enter Finish`, but do not add a second row-selection marker. Persist the order across project switches and application restarts. Keep ordering rules in the domain/storage layer so they can be tested independently of ncurses.

## Responsive Terminal UI

Terminal size must be treated as dynamic.

Handle ncurses `KEY_RESIZE` and recalculate layout whenever dimensions change.

The application must support approximately three presentation levels:

### Wide

Show:

* full task title
* project/context
* priority
* navigation information
* useful secondary metadata

### Compact

When width is reduced, use a dedicated compact layout rather than mechanically cropping the full window. Show Tasks and Notes and hide Projects. Left from Tasks reveals Projects; Right from Tasks shows Notes again. The compact layout may rearrange or simplify pane content while preserving the same data and actions. Remove secondary information and prioritize:

1. task title
2. status
3. priority
4. notes when space permits

### Minimal

When the window is very narrow, use a dedicated minimal layout and show Tasks only. Left and Right replace it with Projects or Notes respectively, following the same non-wrapping `Projects ↔ Tasks ↔ Notes` order. The minimal pane does not need to look like a cropped full-size pane.

When height is limited, omit the hotkey footer to preserve task rows. Press `?` to open the standard centered Help overlay containing the applicable bindings; close it with `?` or `Esc` and return to the underlying pane.

Preserve core operation even when very little screen space is available.

Do not simply squeeze the wide interface into fewer columns.

Use **progressive disclosure**.

A permanent sidebar should be avoided because it wastes significant horizontal space in narrow tiling-WM layouts.

Project selection should instead use a temporary project-switching interface.

#### Minimum usability target

Primary functionality should remain usable at approximately:

```text
40 columns × 12 rows
```

This includes:

* task navigation
* task creation
* task editing
* completion
* project switching
* view switching
* help access

The interface does not need to be visually ideal at this size, but it must remain functional.

Long task titles must be truncated or otherwise handled safely. Rendering must never assume a minimum terminal width without checking it.

## Window templates

* [Full-size main window](templates/template-fullsize-main-window.md): project, task, and notes panes with aligned hotkey columns.
* [Compact main window](templates/template-compact-main-window.md): Tasks and Notes with Projects revealed through pane navigation.
* [Minimal main window](templates/template-minimal-main-window.md): Tasks-only `40×12` layout with pane replacement.
* [Task form](templates/template-task-form.md): shared create/edit form for tasks and subtasks.
* [New Project form](templates/template-new-project.md): one-field project creation with immediate selection.
* [Move task popup](templates/template-task-move.md): destination-project list opened with `m` in the Tasks pane.
* [Help overlay](templates/template-help-overlay.md): centered keyboard reference opened with `?`.

## Pane focus

Highlight only the active pane's heading with reverse video: swap its foreground and background colors, including one space on each side of the heading text. Move this highlight immediately when keyboard focus changes. Other headings and all borders retain their normal appearance.

Use `>` for the selected project and task rows everywhere; do not introduce a second selected-row marker. The markers identify selection independently of pane focus. Task text retains its priority foreground colors. Update the contextual `i` label in the hotkey pane when focus changes, preserving column alignment.

Terminal implementation constraints are recorded in the [ncurses implementation notes](developer/ncurses-ui.md).
