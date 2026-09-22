# Task Form

[UI specification](../ui.md#task-and-subtask-forms) · [Main window](template-fullsize-main-window.md)

The same form serves tasks and subtasks. Name edits the task's title; Priority offers P1/P2/P3. New tasks and subtasks start with P3 selected; editing shows the saved priority. Notes are edited in the main Notes pane.

Up/Down moves between Name and Priority; Tab cycles Name → Priority → Name. Left/Right moves the cursor in Name or changes the selected value in Priority. `1`/`2`/`3` also select P1/P2/P3 when Priority is focused. Enter always saves or creates the form; it does not select a priority. The read-only Parent line is skipped. Arrow keys stay within the form rather than switching main-window panes.

## Edit task

```text
┌─ Edit task ──────────────────────────────────────────────┐
│ Name:     [Implement project discovery               ]   │
│ Priority: [P1]  P2  P3                                   │
│                                                          │
│ ↑/↓ Field          Tab Cycle       ←/→ Cursor/Priority   │
│ Enter Save         Esc Cancel                            │
└──────────────────────────────────────────────────────────┘
```

Enter saves and closes the form from either field. Esc closes it without saving changes. Existing subtasks remain attached and are managed in the main task list.

## New task and subtask

For a new task, use the same form with the title `New task`, a blank Name, P3 selected, and `Enter Create` / `Esc Cancel` help. For a new subtask, include a read-only Parent line:

```text
┌─ New subtask ────────────────────────────────────────────┐
│ Parent: Implement project discovery                      │
│ Name:   [Query registered paths                      ]   │
│ Priority: P1  P2  [P3]                                   │
│                                                          │
│ ↑/↓ Field          Tab Cycle       ←/→ Cursor/Priority   │
│ Enter Create       Esc Cancel                            │
└──────────────────────────────────────────────────────────┘
```

Editing an existing subtask uses the title `Edit subtask`, its saved values and read-only Parent line, with `Enter Save` / `Esc Cancel` help. Failed saves keep the form open and retain the input. Digit shortcuts apply only in the Priority field; Name accepts digits normally. Enter submits from either editable field.

These are form-content references; fit the form to the available terminal space rather than assuming the illustrated width is always available.
