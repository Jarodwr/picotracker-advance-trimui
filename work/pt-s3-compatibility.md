---
id: s3-compatibility
title: Check project and sample compatibility
status: done
priority: high
estimate: L
depends_on: [s3-boot]
write_scope: [planning/findings/sprint-3-compatibility.md, fixtures/]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-compatibility
attempts: 1
---

## Goal

Check project and sample compatibility for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] A version-matched project and WAV fixture load from a disposable copy, round-trip checks are recorded, and capacity behavior is measured.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- planning/findings/sprint-3-compatibility.md, fixtures/

## Result

`tests/compatibility_check.py` creates a project with the pinned firmware, reloads it in a second process, and checks unchanged project bytes. It generates compatible PCM WAV fixtures in a disposable root; the small sample loads and the large sample is rejected at a 1 MiB limit. File hashes remain unchanged. See `planning/findings/sprint-3-compatibility.md` for scope and limits.
