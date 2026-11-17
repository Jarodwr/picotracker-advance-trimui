---
id: s4-midi-devices
title: Implement macOS MIDI devices
status: ready
priority: high
estimate: L
depends_on: [s4-runtime-contracts]
write_scope: [platform/macos/CoreMidiIn.h, platform/macos/CoreMidiIn.cpp, platform/macos/CoreMidiOut.h, platform/macos/CoreMidiOut.cpp]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-midi-devices
attempts: 0
---

## Goal

Provide wrapper-owned CoreMIDI input and output devices for the pinned upstream interfaces.

## Acceptance criteria

- [ ] Input and output adapters map CoreMIDI packets to the upstream device contracts, preserve timestamps where supported, discover and reconnect ports, and tear down callbacks safely. Virtual ports permit testing without physical hardware.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- CoreMidiIn/Out header and implementation files under platform/macos.

## Result

Pending.
