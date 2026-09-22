# New Project Form

[UI specification](../ui.md#project-switching) · [Project requirements](../requirements.md#project-creation-and-persistence) · [Main window](template-fullsize-main-window.md)

The New Project form contains only Project name. When an unregistered directory provides provisional context, prefill the field with that directory's basename—for example, `atomrpc` from `~/work/atomrpc`. The name is editable, so the user can replace the default before saving. When creating a named project from an existing project, start with an empty field.

```text
┌─ New Project ────────────────────────────────────────────┐
│ Project name: [atomrpc                              ]    │
│                                                          │
│ ←/→ Cursor         Enter Create       Esc Cancel         │
└──────────────────────────────────────────────────────────┘
```

Enter saves the project and immediately selects it in the Projects pane. Esc cancels and creates no project. The form must not contain task, priority, notes, or directory-path fields; the directory association comes from the launch context.

If the current directory is a provisional project location, the saved project keeps its canonical directory path while using the edited name as its display name.
