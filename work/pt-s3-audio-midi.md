---
id: s3-audio-midi
title: Implement silent audio and MIDI adapters
status: done
priority: high
estimate: L
depends_on: [s3-source-rebase]
write_scope: [platform/desktop/HostAudio.h, platform/desktop/HostAudio.cpp, platform/desktop/HostMidi.h, platform/desktop/HostMidi.cpp]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-audio-midi
attempts: 1
---

## Goal

Implement silent audio and MIDI adapters for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] The upstream application boots with interface-correct silent audio and MIDI services and reports unsupported hardware cleanly.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- platform/desktop/HostAudio.h, platform/desktop/HostAudio.cpp, platform/desktop/HostMidi.h, platform/desktop/HostMidi.cpp

## Result

Installed before `Application::Init()`; the actual Song view renders and the host output reports `silent` / `No audio device`. No physical MIDI endpoints are registered. See ADR 0010 and the boot finding.
