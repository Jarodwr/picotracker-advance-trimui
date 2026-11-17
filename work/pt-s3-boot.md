---
id: s3-boot
title: Boot Application and verify a real view
status: done
priority: critical
estimate: XL
depends_on: [s3-filesystem, s3-input, s3-audio-midi, s3-samples]
write_scope: [platform/desktop/main.cpp, CMakeLists.txt, planning/findings/sprint-3-boot-result.md]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-boot
attempts: 1
---

## Goal

Boot Application and verify a real view for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] Application::Init renders an actual firmware view on macOS and one SDL key changes that view through upstream input dispatch.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- platform/desktop/main.cpp, CMakeLists.txt, planning/findings/sprint-3-boot-result.md

## Result

The host entry point now calls `Application::Init()`. The Song view was captured in `build/first-firmware-view.png`. The headless smoke dispatches Down and checks a changed renderer hash. See `planning/findings/sprint-3-boot-result.md` and ADR 0011.
