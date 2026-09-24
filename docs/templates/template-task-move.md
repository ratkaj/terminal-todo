# Move Task

[UI specification](../ui.md#move-task) · [Task requirements](../requirements.md#moving-tasks-between-projects) · [Main window](template-fullsize-main-window.md)

Press `m` on a top-level task in the Tasks pane to open this centered popup. It names the task and lists the projects it can move to: non-archived projects other than the current one, in Projects-pane order (built-ins first). A provisional (unsaved) project is not listed.

```text
┌─ Move task ────────────────────────────────────┐
│ Task: Implement project discovery              │
│                                                │
│ > Inbox                                        │
│   This Week                                    │
│   Today                                        │
│   panzerpi                                     │
│   terminal-todo                                │
│                                                │
│                                                │
│                                                │
│                                                │
│ Up/Down Select  Enter Move  Esc Cancel         │
└────────────────────────────────────────────────┘
```

Up/Down moves `>`; the list scrolls when it is longer than the popup. Enter moves the task and closes the popup; Esc closes it without changing anything. When there is nowhere to move the task, the list shows `No other projects`.
