# Compact Main Window

[UI specification](../ui.md#responsive-terminal-ui) · [Full-size main window](template-fullsize-main-window.md) · [Minimal main window](template-minimal-main-window.md)

This compact layout is a dedicated design for reduced width. It shows Tasks and Notes; Projects is hidden. Tasks remains the central working pane. Left from Tasks reveals Projects, and Right from Tasks shows Notes again. The visual composition may differ from the full-size window, while the data, selection markers, pane order, and actions remain the same.

The active pane heading uses reverse video. The selected task uses `>`. The footer columns are aligned across both rows. Archive labels change contextually as specified in the [archive controls](../ui.md#archive-controls).

```text
┌──────────────────────────────────────────────────┬───────────────────────────┐
│ TASKS (atomrpc)                                  │ NOTES                     │
├──────────────────────────────────────────────────┼───────────────────────────┤
│ > [ ] Implement project discovery       P1       │ Implement project discover│
│       [ ] Query registered paths                 │                           │
│       [x] Walk parent directories                │ Review on Sep 24          │
│   [ ] Improve ncurses UI                P2       │ Requirements:             │
│       [ ] Handle window resize                   │ - Handle filesystem root  │
│   [ ] Write initial test suite          P3       │ - Use canonical paths     │
│                                                  │                           │
├──────────────────────────────────────────────────┼───────────────────────────┤
│ ←/→ Panes   ↑/↓ Navigate       i Insert   Space Done   a Archive completed   │
│ o Order     A Display archived  ? Help     q Quit                            │
└──────────────────────────────────────────────────────────────────────────────┘
```

The project pane is shown in this same layout area when reached with Left from Tasks. Do not add a permanent project sidebar solely to keep it visible.
