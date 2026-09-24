# Contributing

This project welcomes contributions from humans and from AI coding agents
working on a human's behalf — it was built almost entirely the second way,
and its documentation structure reflects that on purpose.

## Start here, whoever/whatever you are

If you're an AI agent (Claude Code, or anything else that reads
`AGENTS.md`), start at [`AGENTS.md`](AGENTS.md) (a symlink to
[`CLAUDE.md`](CLAUDE.md)). It tells you the required reading order:
[`docs/agent-lessons.md`](docs/agent-lessons.md) first (a running log of
mistakes and corrections from this project's own development — read it
before you repeat one), then [`README.md`](README.md), then the relevant
spec docs for whatever you're touching.

If you're a human, the short version of the same map:

| Doc | What it's for |
| --- | --- |
| [`docs/requirements.md`](docs/requirements.md) | Functional spec: projects, tasks, subtasks, priorities, ordering, persistence. |
| [`docs/ui.md`](docs/ui.md) | Hotkeys, layout, forms, responsive behavior. |
| [`docs/archiving_and_ordering.md`](docs/archiving_and_ordering.md) | The one genuinely fiddly subsystem, specified in detail. |
| [`docs/development.md`](docs/development.md) | Architecture rules, testing checklist, code-quality bar. |
| [`docs/developer/ncurses-ui.md`](docs/developer/ncurses-ui.md) | ncurses-specific implementation notes (Unicode, color, resize, the `$EDITOR` hand-off). |
| [`docs/developer/ARCHITECTURE.md`](docs/developer/ARCHITECTURE.md) | The module breakdown, SQLite schema, and UI state machine. |

These read like a specification, not a user manual, because that's
literally what they are: this app was built by writing the spec first and
having an AI agent implement against it. If you're extending the app,
update the relevant spec doc alongside the code — that's what keeps the
next agent (human or AI) from re-deriving decisions that were already made.

## The AI-assisted development model

From `docs/development.md`, unchanged, because it's still the rule:

The human developer retains responsibility for architecture, requirements,
interfaces, UX decisions, testing strategy, acceptance criteria, and code
review. The AI agent may implement bounded components, tests, SQLite
integration, ncurses plumbing, build-system changes, refactoring,
documentation, and static-analysis fixes. The AI must not introduce major
architectural changes merely because they simplify implementation, and must
ask rather than invent behavior when a decision materially affects
architecture or UX.

## Building and testing

```bash
autoreconf -i
./configure --enable-tests --enable-werror
make
make tests
```

Two more build modes matter before anything ships:

```bash
# Address/Undefined Behavior Sanitizer - must be a clean run, 0 leaks
./configure --enable-tests --enable-sanitize --enable-werror && make && make tests

# Coverage report (docs/development.md: task/project/storage matter most;
# don't chase 100% as a vanity metric)
./configure --enable-tests --enable-coverage && make coverage
```

Every build compiles with `-Wall -Wextra -Wpedantic`. Development builds and
CI add `--enable-werror`, so a warning is a build failure, not a suggestion.
It is off by default so that a newer compiler's new warnings don't break a
user's build from source.

## Screenshots

The README screenshots are generated from the real binary, so refresh them
whenever a UI change shows up in them:

```bash
make && scripts/screenshots.py
```

It needs `tmux` and `google-chrome` or `chromium`. The script runs `src/todo`
in a detached tmux session against a temporary `HOME` seeded with example
data, so your own database is never touched. To add a shot, append its name
and key sequence to `SHOTS` in the script.

## Commit style

This repo uses [Conventional Commits](https://www.conventionalcommits.org/)
throughout its history: `type(scope): summary`, imperative mood, no
trailing period, body wrapped at 72 columns explaining *why* rather than
*how*. `git log --oneline` is the best style reference.

## Scope

Read [`README.md`](README.md)'s "Non-goals" section before proposing a
feature. This is deliberately a small, single-user, keyboard-only tool —
"does this make capturing, finding, planning, or completing my tasks
meaningfully faster?" is the actual bar, not "would this be a nice
feature."
