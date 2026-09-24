# Agent Lessons and Working Rules

Read this at the start of each session, before substantive work. After context loss, reload it if its contents are no longer available. This is a working log for better communication and lower overhead, not a transcript.

## Working rules

* Read the relevant sections, not whole documents repeatedly. Batch independent reads and keep tool output focused.
* Apply authorized changes without repeatedly seeking permission. Ask only about unresolved decisions that materially affect behavior or scope.
* Separate user-approved requirements from agent proposals. Do not silently turn plausible defaults into product decisions.
* Give each rule one authoritative home. When changing it, check related specifications, examples, templates, and agent instructions for contradictions.
* Check symlink targets before editing. Consider restrictions on both the named file and the content reached through it.
* Use tools and skills only when they help the actual deliverable. Terminal color samples do not require an inline-visualization workflow.
* Generate aligned layouts from content widths. Distinguish byte count, character count, and terminal-cell width.
* When adding alternative controls, retain existing supported shortcuts in contextual help unless the user explicitly removes them. Keep Tab visible alongside arrows; the user prefers forward cycling over a separate reverse-Tab shortcut for this small form. Use multiple help lines when needed.
* Report exactly what was checked. Static alignment checks are not visual inspection or runtime testing.
* Make form key semantics explicit in both prose and mockups: distinguish field selection, value changes, and submit/cancel actions.
* Keep updates and final responses short. State the result and material limitations; avoid repeating settled choices or routine check inventories.

## Mistake log — 2026-09-21

| Mistake | Correction for future work |
| --- | --- |
| Left date-column requirements after dates moved to notes; retained conflicting completion keys. | Check agreement across all affected documents, not just the edited paragraph. |
| Recorded inferred subtask ordering and CLI syntax as settled behavior. | Label proposals and resolve material ambiguities before finalizing them. |
| Edited `CLAUDE.md` without accounting explicitly for the earlier restriction on its `AGENTS.md` symlink. | Recognize indirect edits and evaluate authorization before changing shared content. |
| Read a large visualization skill for simple ANSI previews. | Choose the smallest suitable workflow. |
| Repeatedly printed full specifications for small edits. | Use targeted searches and section reads after initial orientation. |
| Manually packed a footer that exceeded its width. | Calculate column widths and available space before writing. |
| Described character-count checks too broadly as verified visual alignment. | State the verification method and its limits. |
| Repeated accepted bindings and routine verification details in reports. | Report only useful changes, unresolved issues, and meaningful evidence. |

## Mistake log — 2026-09-22

| Mistake | Correction for future work |
| --- | --- |
| Relabeled P2 as normal while reconciling a new document. | Preserve the established distinction: P3 is the default with no added foreground color; P2 is elevated and yellow. |

## Mistake log — 2026-09-24

| Mistake | Correction for future work |
| --- | --- |
| Ran two dependent `git commit` steps as parallel tool calls; the first failed its length check and the second committed every staged file under the wrong message. | Run commit steps in sequence, and check each one succeeded before staging the next. |

## Known follow-ups

These are unresolved observations, not authorization to change requirements:

* Sibling-only reordering and `--priority P1` CLI syntax originated as agent interpretations; confirm when those behaviors are next discussed.
* Text-entry fields (task/project name, etc.) read input via `wgetch()`, which delivers multi-byte UTF-8 keystrokes one byte at a time; typing a non-ASCII character (accents, CJK, emoji) currently drops or mangles it rather than inserting it correctly. Found 2026-09-22 while visually verifying the step-17 Unicode/`wcwidth` rendering fix (which is unaffected — it only corrects how already-stored titles are *drawn/truncated*, not how they're *typed*). Fixing input would mean switching `input_dispatch_key()`'s `int key` API to wide-character input (`get_wch()`), touching every call site and test — not attempted as part of that polish pass; flagging here for a future scoped pass.

* Scrolled Tasks/Projects lists show no indicator that rows are hidden above or below. Found 2026-09-24; not yet requested.

## Maintaining this log

Add material mistakes and actionable user feedback when they occur. Merge repeated lessons, remove resolved follow-ups, and keep this file concise. Do not append routine successes, every tool retry, or repeated summaries of the same issue. User instructions take precedence over this log.
