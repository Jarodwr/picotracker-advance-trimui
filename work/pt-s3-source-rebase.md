---
id: s3-source-rebase
title: Migrate to the dedicated Advance source
status: done
priority: critical
estimate: L
depends_on: []
write_scope: [.gitmodules, upstream/advance-firmware, CMakeLists.txt, platform/desktop, planning/findings/source-migration.md, docs/adr]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-source-rebase
attempts: 1
---

## Goal

Adopt the official dedicated Advance source as a reproducible submodule baseline.

## Acceptance criteria

- [x] Compare v3.2.1 with the current source, including licensing and recursive dependencies.
- [x] Pin the dedicated repository without editing its source tree and restore a passing host scaffold build and smoke check.
- [x] Document new source architecture and remaining boot gaps.

## Inputs

- `xiphonics/advance-firmware`, ADR 0007, and the existing Sprint 2 host prototype.

## Output

- Updated submodule and `planning/findings/source-migration.md`.

## Result

Pinned the dedicated repository at v3.2.1 and renamed its submodule path. Updated the wrapper's CMake source path and host shims, built the model and view probes from a clean host build directory, and passed the SDL dummy-video smoke check. The submodule is clean. Architecture and license differences are recorded in `planning/findings/source-migration.md` and ADR 0007. Actual application boot remains dependent on host services.
