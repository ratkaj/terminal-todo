# Full-size Main Window

[UI specification](../ui.md) · [Project overview](../../README.md)

This example has keyboard focus in Tasks. Render only the `TASKS (atomrpc)` heading, with one space on each side, in reverse video; plain Markdown cannot show that attribute. Other headings and borders stay normal, and task text uses the accepted priority colors. The `>` markers indicate selection independently of focus. The separate application/project title row is omitted; the current project appears in the Tasks heading.

When focus changes, move the reversed heading and change the `i` label to `i New project` in Projects, `i Insert` in Tasks, or `i Edit notes` in Notes. Keep both hotkey rows aligned in columns.

The `a` and `A` labels are contextual. This normal Tasks view uses `a Archive completed` and `A Display archived`; when an archived task is selected they become `a Restore` and `A Hide archived`. Projects uses the corresponding project labels. Notes has no archive action.

The `d Delete` action follows the focused pane: projects, tasks, or notes. For Today, This Week, and Inbox it clears tasks while retaining the destination. Use the [single y/n/Y confirmation](../ui.md#deletion), including its session-only, per-category suppression.

```text
┌───────────────────────┬───────────────────────────────────────────────────────────────┬──────────────────────────────────────┐
│ PROJECTS              │ TASKS (atomrpc)                                               │ NOTES                                │
├───────────────────────┼───────────────────────────────────────────────────────────────┼──────────────────────────────────────┤
│                       │                                                               │                                      │
│   Inbox           (3) │ > [ ] Implement project discovery                P1           │ Implement project discovery          │
│   Today           (5) │       [ ] Query registered paths                              │                                      │
│   This Week      (12) │       [ ] Add tests for filesystem root                       │ Walk up from current directory to    │
│                       │       [x] Walk parent directories                             │ find the nearest registered project  │
│ > atomrpc         (7) │                                                               │ root.                                │
│   homelab         (4) │   [ ] Improve ncurses UI                         P2           │ Review on Sep 24                     │
│   panzerpi        (2) │       [ ] Handle window resize                                │ Requirements:                        │
│   personal        (5) │       [ ] Design color scheme                                 │ - Handle filesystem root correctly   │
│   work            (1) │       [ ] Implement help screen                               │ - Do not match partial paths         │
│                       │                                                               │ - Use canonical paths                │
│                       │   [ ] Write initial test suite                   P3           │                                      │
│                       │                                                               │ References:                          │
│                       │   [ ] Set up CI build                            P3           │ - test_project.c                     │
│                       │                                                               │ - realpath(3)                        │
│                       │   [ ] Add fast CLI capture                       P3           │                                      │
│                       │                                                               │                                      │
│                       │   [ ] Write project documentation                P3           │                                      │
│                       │                                                               │                                      │
│                       │   [x] Research SQLite schema                     P3           │                                      │
│                       │                                                               │                                      │
│                       │                                                               │                                      │
│                       │                                                               │                                      │
├───────────────────────┴───────────────────────────────────────────────────────────────┴──────────────────────────────────────┤
│ ←/→ Panes        ↑/↓ Navigate   p Projects      i Insert   Space Done   a Archive completed   A Display archived             │
│ 1/2/3 Priority  o Order  n Edit notes  Enter Open/Edit  r Rename  s Subtask  d Delete  ? Help  q Quit                        │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```
