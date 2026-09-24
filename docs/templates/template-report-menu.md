# Generate Report

[UI specification](../ui.md#reports) · [Report requirements](../requirements.md#reports) · [Main window](template-fullsize-main-window.md)

Press `g` in any pane to open this centered popup. It lists the report periods. Up/Down moves `>`, Enter opens the report for the highlighted period read-only in `$EDITOR`, and Esc closes the popup without generating anything. The popup always opens on `This week`.

```text
┌─ Generate report ────────────────────┐
│ Tasks completed:                     │
│                                      │
│ > This week                          │
│   Last week                          │
│   This month                         │
│   Last month                         │
│                                      │
│ Enter Open  Esc Cancel               │
└──────────────────────────────────────┘
```

The report format is specified in [Reports](../ui.md#reports).
