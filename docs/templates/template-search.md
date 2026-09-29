# Search

[UI specification](../ui.md#search) · [Search requirements](../requirements.md#search) · [Main window](template-fullsize-main-window.md)

Press `/` in any pane to open this centered popup. It uses most of the window, up to 100×26. The query is on the first row. Results are listed on the left, and the highlighted result's context is on the right.

```text
┌─ Search ─────────────────────────────────────────────────────────────────────────────────────────┐
│ / mqtt                                                                                           │
├────────────────────────────────────────────────┬─────────────────────────────────────────────────┤
│  [ ] P2 Implement MQTT ACL handling a very lon │ atomrpc                                         │
│> [x] P3 Capture MQTT traffic (archived)        │ Investigate broker reconnect ›                  │
│  [ ] P3 Test MQTT recovery                     │                                                 │
│  [ ] P3 Old MQTT bridge idea                   │ Capture MQTT traffic                            │
│  [ ] P3 Write docs                             │                                                 │
│                                                │ Saw two CONNECTs per reconnect in the           │
│                                                │ capture; compare with the broker log.           │
│                                                │                                                 │
├────────────────────────────────────────────────┴─────────────────────────────────────────────────┤
│ Up/Down Select  Enter Open  Esc Close                                                            │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

* **Result row:** `>` on the highlighted row, the checkbox, the priority, and the title cut to fit. Archived tasks keep ` (archived)` visible at the end. The whole row uses the task's priority colour.
* **Context:**
  * the project name, with ` (archived)` for an archived project;
  * for a subtask, the parent's title followed by `›`;
  * a blank row, then the full title, word-wrapped;
  * a blank row, then the notes, word-wrapped like the Notes pane.
* **Empty query:** the list shows `Type to search all tasks`.
* **No results:** the list shows `No matching tasks`.

When the popup is narrower than 60 inner columns, the context side is left out and the list uses the full width. Where the key hint does not fit, it shortens to `Up/Dn Select`:

```text
┌─ Search ───────────────────────────┐
│ / mqtt                             │
├────────────────────────────────────┤
│  [x] P3 Capture MQTT tra (archived)│
│  [ ] P3 Test MQTT recovery         │
│  [ ] P3 Old MQTT bridge idea       │
│> [ ] P3 Write docs                 │
├────────────────────────────────────┤
│ Up/Dn Select  Enter Open  Esc Close│
└────────────────────────────────────┘
```

Every other key types into the query, including `q`, `?`, `/` and digits. Backspace deletes a character, Up/Down moves `>`, and the list scrolls to keep it visible. Enter opens the highlighted task, as described in [Search](../ui.md#search); with no results it does nothing. Esc closes the popup without changing anything.
