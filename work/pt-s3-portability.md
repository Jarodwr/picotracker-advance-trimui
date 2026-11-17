---
id: s3-portability
title: Validate portability and patch strategy
status: blocked
priority: medium
estimate: L
depends_on: [s3-boot]
write_scope: [planning/findings/sprint-3-portability.md, docs/adr/]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-portability
attempts: 1
---

## Goal

Validate portability and patch strategy for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [ ] Linux and Windows builds are attempted, remaining platform gaps are recorded, and narrow upstream patch candidates are identified.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- planning/findings/sprint-3-portability.md, docs/adr/

## Result

The Ubuntu 24.04 build configured and exposed a GCC incompatibility in the temporary `strlen` macro. The wrapper compile options were narrowed, and macOS rebuilt successfully. The proposed upstream patch built and passed smoke/compatibility in a disposable source copy with the macOS workaround disabled; the pinned submodule stayed clean. The Linux recheck was stopped when automatic approval review hit its usage limit for Docker actions. A Windows GNU-target configuration reached SDL2 package generation but found only a macOS SDL2 library; no Windows SDK or import library is installed. A local macOS check script now covers the runnable port. Linux and Windows builds are deferred at the user's request. Findings and patch candidates are in `planning/findings/sprint-3-portability.md` and ADR 0012. This job remains open until those builds are verified.

Roadmap order: complete Sprint 4, macOS feature completeness, and macOS release readiness before resuming this job for Linux and Windows.
