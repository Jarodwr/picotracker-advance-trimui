---
id: s4-service-integration
title: Integrate host audio and MIDI services
status: ready
priority: critical
estimate: M
depends_on: [s4-audio-output, s4-midi-devices]
write_scope: [platform/desktop/HostAudio.h, platform/desktop/HostAudio.cpp, platform/desktop/HostMidi.h, platform/desktop/HostMidi.cpp, platform/desktop/main.cpp, platform/macos/MacBackend.h, platform/macos/MacBackend.cpp, CMakeLists.txt]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-service-integration
attempts: 0
---

## Goal

Install SDL audio and CoreMIDI services before the real firmware application starts.

## Acceptance criteria

- [ ] The macOS executable links and boots with an available output and virtual MIDI ports; the same executable still boots when devices are absent. Device identities are visible to upstream settings, and all shared startup/CMake edits have one owner.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- Integrated host service selection and CMake dependencies.

## Result

Pending.
