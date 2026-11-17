---
id: s3-filesystem
title: Implement a sandboxed host filesystem
status: done
priority: critical
estimate: L
depends_on: [s3-source-rebase]
write_scope: [platform/desktop/HostFileSystem.h, platform/desktop/HostFileSystem.cpp]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-filesystem
attempts: 1
---

## Goal

Implement a sandboxed host filesystem for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] Firmware paths resolve inside a dedicated host data directory and cannot escape it. Open, list, create, delete, and move operations satisfy the upstream interface.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- platform/desktop/HostFileSystem.h, platform/desktop/HostFileSystem.cpp

## Result

Implemented `HostFileSystem` and installed it in the SDL scaffold. The check exercises read/write, directory creation and listing, case-insensitive filters, list truncation, copy/move/delete, parent traversal and a dangling symlink escape. A clean host build, the filesystem check and SDL smoke passed. See ADR 0008. Changed files: `platform/desktop/HostFileSystem.h`, `.cpp`, `main.cpp`, `CMakeLists.txt`, `tests/host_filesystem_check.cpp`, README and ADR 0008.
