# Help Overlay

[UI specification](../ui.md#responsive-terminal-ui) · [Main window](template-fullsize-main-window.md)

Press `?` from navigation mode to show this centered overlay. It is the same Help window at every terminal height; on short windows it replaces the hidden hotkey footer. Press `?` or `Esc` to close it and return to the underlying pane.

```text
┌─ Help ───────────────────────────────────────────────────────┐
│ ←/→ Panes        ↑/↓ Navigate       p Projects               │
│ i Insert/Edit    n Edit notes       s Subtask                │
│ Enter Open/Edit  Space Done         d Delete/Clear           │
│ 1/2/3 Priority   o Order            r Rename                 │
│ a Archive done   A Show archived                             │
│ e Export         m Move to project  Esc Save/Cancel          │
│ q Quit           ? Close                                     │
└──────────────────────────────────────────────────────────────┘
```

The overlay uses the global aligned-hotkey-column rule. Its contents should reflect the currently available navigation actions and active mode, including the contextual archive labels for the focused pane and selection.
