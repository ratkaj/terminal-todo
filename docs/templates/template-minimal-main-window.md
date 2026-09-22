# Minimal Main Window

[UI specification](../ui.md#responsive-terminal-ui) · [Compact main window](template-compact-main-window.md) · [Help overlay](template-help-overlay.md)

This minimal layout targets approximately `40×12`. It shows Tasks only. Left replaces Tasks with Projects; Right replaces Tasks with Notes. Pane navigation remains bounded and follows `Projects ↔ Tasks ↔ Notes` without wrapping.

The active pane heading uses reverse video and the selected row uses `>`. The hotkey footer is omitted at this height; `?` opens the centered [Help overlay](template-help-overlay.md).

```text
┌─────────────────────────────────────────┐
│ TASKS (atomrpc)                         │
├─────────────────────────────────────────┤
│ > [ ] Implement project discovery   P1  │
│       [ ] Query registered paths        │
│       [x] Walk parent directories       │
│   [ ] Improve ncurses UI            P2  │
│       [ ] Handle resize                 │
│   [ ] Write test suite              P3  │
│                                         │
│                                         │
└─────────────────────────────────────────┘
```

Long titles are truncated by terminal-cell width. Preserve task navigation, completion, creation, editing, archiving/restoration, archive visibility, ordering, and project/notes pane replacement at this size. The Help overlay provides the archive bindings while the footer is hidden.
