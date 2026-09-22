# Personal ncurses Task Manager

Build a small, fast, keyboard-driven personal task manager for Linux using **C and ncurses**.

This is a personal productivity tool, not a general-purpose project-management application.

The primary design priorities are:

1. Excellent terminal user experience.
2. Very fast task capture and manipulation.
3. Directory-aware project context.
4. Responsive operation in terminals of widely varying sizes.
5. Simple, maintainable architecture.
6. Reliable persistence and strong automated testing.

Avoid unnecessary features and abstractions.

## Core concept

Enter a project directory and run `todo` to work with its tasks:

```bash
cd ~/work/atomrpc
todo
```

Projects can also exist without a directory, including the manually organized `Today` and `This Week` projects. Tasks live in a central SQLite database rather than project-local files.

Running `todo` from your home directory opens `Today`.

## Documentation

The repository currently contains requirements and UI templates. Build and run instructions will be added when an implementation is available.

| Document | Contents |
| --- | --- |
| [Functional requirements](docs/requirements.md) | Projects, tasks, subtasks, notes, priorities, ordering, persistence, CLI capture, and search. |
| [Archiving and ordering](docs/archiving_and_ordering.md) | Task and project archiving, archive visibility, restoration, and state-aware ordering. |
| [User interface](docs/ui.md) | Navigation, hotkeys, colors, reorder mode, responsive behavior, and template links. |
| [Development requirements](docs/development.md) | Existing architecture constraints, testing and code quality targets, and AI-assisted development guidelines. |
| [ncurses implementation notes](docs/developer/ncurses-ui.md) | Rendering, Unicode, colors, resizing, and terminal-specific pitfalls. |
| [Window templates](docs/templates/) | Layout references, starting with the [full-size main window](docs/templates/template-fullsize-main-window.md). |
| [Implementation plan](docs/developer/implementation-plan.md) | Module breakdown, SQLite schema, build order, and current progress status. |

Product specifications live in `docs/`; C coding and implementation guidance live in `docs/developer/`. Documentation of implemented code architecture will be added as the code takes shape.

## Non-goals

Do not implement unless requirements change:

* collaboration
* multiple users
* cloud synchronization
* calendar integration
* notifications
* attachments
* arbitrary task nesting
* complex dependency graphs
* recurring tasks
* tags
* snoozing/defer system
* elaborate workflow states
* web interface
* GUI
* plugin system

The objective is not to reproduce Todoist, Jira, or another general-purpose task manager.

The objective is a **small Linux tool optimized around one user's workflow: enter a project directory, run `todo`, and immediately work with the tasks relevant to that context.**

## Design Principle

Every proposed feature should be evaluated against:

> Does this make capturing, finding, planning, or completing my tasks meaningfully faster?

If not, leave it out.

Prefer a small application whose complete behavior can be understood over a feature-rich application that requires configuration or maintenance.
