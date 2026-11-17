---
id: s3-input
title: Implement host timer and event input
status: done
priority: critical
estimate: L
depends_on: [s3-source-rebase]
write_scope: [platform/desktop/HostTimer.h, platform/desktop/HostTimer.cpp, platform/desktop/HostEvent.cpp]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-input
attempts: 1
---

## Goal

Implement host timer and event input for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] An SDL key travels through an upstream `GUIWindow` probe and changes its cursor. Timers use a monotonic host clock. The firmware cursor remains in `s3-boot`.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- platform/desktop/HostTimer.h, platform/desktop/HostTimer.cpp, platform/desktop/HostEvent.cpp

## Result

Implemented `HostTimerService`, `HostEventManager` and `HostWindowFactory`; installed them in the SDL scaffold. A synthetic Down key dispatches through upstream `GUIWindow` and advances a probe cursor; key release and a monotonic timer observer tick also pass. The actual firmware cursor remains unverified until application boot. See ADR 0009. Changed files: `platform/desktop/HostTimer.*`, `HostEvent.*`, `SDLWindow.*`, `main.cpp`, `CMakeLists.txt`, `tests/host_event_check.cpp`.
