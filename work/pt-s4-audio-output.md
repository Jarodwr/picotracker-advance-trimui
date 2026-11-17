---
id: s4-audio-output
title: Implement SDL audio output
status: ready
priority: high
estimate: L
depends_on: [s4-runtime-contracts]
write_scope: [platform/desktop/SDLAudioDriver.h, platform/desktop/SDLAudioDriver.cpp]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-audio-output
attempts: 0
---

## Goal

Provide a wrapper-owned SDL AudioDriver and reuse upstream AudioOutDriver without changing the firmware submodule.

## Acceptance criteria

- [ ] The adapter implements every AudioDriver operation, queues upstream-rendered PCM at the expected sample rate and format, reports actual device state, and handles start/stop and device loss without a data race. It can be compiled independently of application startup.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- SDLAudioDriver.h and SDLAudioDriver.cpp.

## Result

Pending.
