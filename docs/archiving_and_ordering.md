# Task and Project Archiving, Ordering, and Display

[Project overview](../README.md) · [Functional requirements](requirements.md) · [User interface](ui.md)

This document expands the archive and ordering rules in the functional requirements. The functional requirements define the data model; the UI guide defines keyboard and presentation behavior.

## 1. General Model

Completion, archiving, priority, manual ordering, and archive visibility are separate concepts.

A task retains its priority and other metadata regardless of whether it is completed or archived.

Archiving must not delete data or modify task priority.

Displaying or hiding archived items is purely a UI operation. It does not modify the archived state of any task or project.

---

## 2. Task States

Tasks effectively belong to one of three groups:

1. **Active** — not completed and not archived.
2. **Completed** — completed but not archived.
3. **Archived** — archived, regardless of whether the task was previously completed.

An archived task may internally retain its completed state. Archiving does not reset or otherwise modify completion state.

The archived state takes precedence for display grouping.

```text
completed = false, archived = false  -> ACTIVE
completed = true,  archived = false  -> COMPLETED
completed = false, archived = true   -> ARCHIVED
completed = true,  archived = true   -> ARCHIVED
```

---

## 3. Completing Tasks

Completing a task and archiving a task are separate operations.

`Space` toggles the completion state of the selected task.

When a task is completed:

* mark it as completed;
* preserve its priority;
* preserve its notes and other metadata;
* do not automatically archive it.

Completed tasks remain visible below active tasks.

A completed task can be returned to active state by toggling its completion state again.

---

## 4. Archiving Completed Tasks

Individual task archiving is not part of the normal workflow.

Instead, the task pane provides an **Archive Completed** operation.

In the normal task view:

```text
a    Archive all completed tasks in the current project
A    Show archived tasks
```

`Archive Completed` applies only to the currently selected project/list.

It MUST NOT archive completed tasks belonging to other projects.

Before archiving, the UI should request confirmation:

```text
Archive 4 completed tasks? y/n/Y
```

If confirmed, all completed, non-archived tasks belonging to the current project are marked as archived. The subtasks of each archived parent are archived with it, including open subtasks.

Their completion state, priority, notes, and other metadata remain unchanged.

Example before:

```text
[ ] Current task                         P1
[ ] Another task                         P2
[x] Finished task A                      P1
[x] Finished task B                      P2
[x] Finished task C                      P3
```

After `Archive Completed`:

```text
[ ] Current task                         P1
[ ] Another task                         P2
```

The completed tasks still exist in the database. They are simply archived and therefore hidden by the normal task display.

---

## 5. Displaying Archived Tasks

Archived tasks are normally hidden.

`A` toggles whether archived tasks are displayed in the current task pane.

This is only a display/filter operation.

It MUST NOT change any task's archived state.

Conceptually:

```text
Normal display:
    active tasks
    completed tasks

Archived displayed:
    active tasks
    completed tasks
    archived tasks
```

Pressing `A` again hides archived tasks.

Use the terminology:

```text
Show archived
Hide archived
```

Do not describe this as entering or leaving an archive. There is no separate archive location or archive mode. Archived items are simply records whose archived state determines whether they are normally displayed.

---

## 6. Restoring Archived Tasks

When archived tasks are being displayed, `a` operates on the selected archived task.

```text
a    Restore selected archived task
A    Hide archived tasks
```

Restoring a task clears its archived state.

It does not change its completion state, priority, notes, or other metadata.

For example:

```text
completed = true
archived  = true
priority  = P1
```

after restoration becomes:

```text
completed = true
archived  = false
priority  = P1
```

The restored task therefore returns to the completed P1 group.

Archiving and restoring keep parent/subtask blocks together: archiving a completed parent also archives its subtasks, restoring a parent also restores its subtasks, and restoring a subtask also restores its parent. See [Archiving and restoration](requirements.md#archiving-and-restoration).

The meaning of `a` is contextual:

```text
Archived hidden:
    a    Archive all completed tasks

Archived displayed + archived task selected:
    a    Restore selected archived task
```

The footer/help text should reflect the currently available operation.

---

## 7. Archiving Projects

Projects can be archived individually.

Archiving a project hides the entire project from the normal project pane.

Archiving a project MUST NOT individually archive, complete, or otherwise modify its tasks.

Conceptually:

```text
project.archived = true
```

is sufficient.

All tasks belonging to the project retain their existing state and metadata.

The permanent built-in destinations `Today`, `This Week`, and `Inbox` cannot be archived.

---

## 8. Displaying Archived Projects

Archived projects are normally hidden from the project pane.

The project pane uses the same archive-key convention:

```text
Archived hidden:
    a    Archive selected project
    A    Show archived projects
```

When archived projects are displayed:

```text
a    Restore selected archived project
A    Hide archived projects
```

Again, showing archived projects is purely a UI filter. It does not change project state.

Do not describe this as entering or leaving an archive.

Use:

```text
Show archived
Hide archived
```

Restoring an archived project clears its archived state. Its tasks remain exactly as they were.

---

## 9. Archive Hotkey Convention

The archive controls should be consistent between the Projects and Tasks panes.

### Projects pane

```text
Archived hidden:
    a    Archive selected project
    A    Show archived projects

Archived displayed:
    a    Restore selected archived project
    A    Hide archived projects
```

If archived projects are displayed but the selected project is not archived, `a` has no action.

### Tasks pane

```text
Archived hidden:
    a    Archive all completed tasks
    A    Show archived tasks

Archived displayed, archived task selected:
    a    Restore selected archived task
    A    Hide archived tasks
```

If archived tasks are displayed but the selected task is not archived, `a` has no action. The footer must not advertise an unavailable archive operation.

The footer must use contextual descriptions so the user does not need to remember what `a` currently means. The `A` label changes between `Show archived` and `Hide archived`.

---

## 10. Priority

Tasks have three priority levels:

```text
P1 = highest priority
P2 = elevated priority
P3 = default priority
```

Priority is persistent task metadata.

Completion and archiving MUST NOT modify priority.

A completed or archived task that was P1 remains P1.

---

## 11. Task Ordering

Tasks are first grouped according to state:

```text
ACTIVE
COMPLETED
ARCHIVED
```

Within each group, tasks are ordered by priority:

```text
P1
P2
P3
```

Therefore the complete logical ordering is:

```text
ACTIVE
    P1
    P2
    P3

COMPLETED
    P1
    P2
    P3

ARCHIVED
    P1
    P2
    P3
```

If priorities are represented numerically:

```text
P1 = 1
P2 = 2
P3 = 3
```

ascending priority ordering produces the desired result.

For each peer list, the effective ordering hierarchy is:

```text
STATE
  ↓
PRIORITY
  ↓
MANUAL ORDER
```

Top-level tasks are one peer list. Each parent's subtasks are a separate peer list; state grouping does not detach a subtask from its parent.

---

## 12. Manual Ordering

The user must be able to manually reorder tasks within the same state and priority group.

Example:

```text
ACTIVE / P1

[ ] Fix MQTT handling
[ ] Finish event manager
[ ] Add integration tests
```

These tasks may be manually reordered.

Manual reordering must not cross a state or priority boundary.

For example, moving the first P2 task upward stops at the P2/P1 boundary.

Changing priority is a separate operation.

Likewise, manual reordering cannot move a completed task into the active group or vice versa.

---

## 13. Order Storage

Tasks should contain the explicit ordering field named in the task model:

```text
manual_order
```

It determines the user's preferred ordering within a state/priority group.

A simple implementation may use spaced integer values:

```text
Task A    manual_order = 10
Task B    manual_order = 20
Task C    manual_order = 30
```

Moving Task B below Task C can simply swap their ordering values:

```text
Task A    10
Task C    20
Task B    30
```

There is no requirement for fractional ordering.

---

## 14. Priority Changes

Changing priority moves a task into a different priority group.

Example:

```text
P1
    Task A
    Task B

P2
    Task C
    Task D
```

Changing Task C from P2 to P1 results in:

```text
P1
    Task A
    Task B
    Task C

P2
    Task D
```

A task whose priority changes should be inserted at the end of the destination priority group.

The user may subsequently manually reorder it.

---

## 15. Completion and Ordering

When an active task becomes completed, it moves from its active priority group into the corresponding completed priority group.

Example:

```text
ACTIVE / P1
    Task A
    Task B
    Task C
```

Completing Task B produces:

```text
ACTIVE / P1
    Task A
    Task C

COMPLETED / P1
    Task B
```

A newly completed task should be inserted at the end of the corresponding completed priority group.

If the task is returned to active state, it should be inserted at the end of the corresponding active priority group.

This makes state transitions deterministic and avoids depending on stale ordering positions from another group.

---

## 16. Subtask Ordering

Subtasks have their own manual ordering within their parent.

Example:

```text
[ ] Implement project discovery
    [x] Walk parent directories
    [ ] Query registered paths
    [ ] Add filesystem-root tests
```

Moving a parent task moves the complete parent/subtask block in the displayed task list.

Subtasks must never be independently reordered relative to unrelated top-level tasks.

Subtask ordering is scoped to the parent task.

Only one level of subtasks is supported.

---

## 17. Normal Task Display

With archived tasks hidden, the task pane displays active tasks followed by completed tasks.

Within both groups, priority and manual ordering apply.

Example:

```text
[ ] Critical implementation work        P1
[ ] Fix project discovery               P1
[ ] Improve tests                       P2
[ ] Documentation                       P3

[x] Implement SQLite schema             P1
[x] Create initial UI prototype         P2
[x] Clean old test code                 P3
```

Completed tasks may be visually dimmed, but task state must never depend solely on color.

---

## 18. Display With Archived Tasks

When archived tasks are explicitly displayed, they appear after active and completed tasks:

```text
ACTIVE
    P1
    P2
    P3

COMPLETED
    P1
    P2
    P3

ARCHIVED
    P1
    P2
    P3
```

Archived tasks follow the same priority and manual-order rules.

Archived projects similarly become visible in the project pane when archived projects are explicitly displayed.

---

## 19. Core Invariants

The implementation must preserve these rules:

* Completion and archiving are independent states.
* Completing a task does not automatically archive it.
* Tasks are archived through `Archive Completed` for the current project.
* Project archiving does not modify the project's tasks.
* Archived items are hidden by default.
* Displaying/hiding archived items is a UI filter only.
* Restoring clears the archived state without modifying other metadata.
* Priority is never implicitly changed by completion or archiving.
* Task ordering follows `state -> priority -> manual order`.
* Manual ordering cannot cross state or priority boundaries.
* `a` performs the context-appropriate archive/restore operation.
* `A` toggles displaying/hiding archived items.
