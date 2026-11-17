---
id: s4-midi-validation
title: Verify MIDI routing and device changes
status: ready
priority: high
estimate: M
depends_on: [s4-service-integration]
write_scope: [tests/macos_midi_check.cpp, planning/findings/sprint-4-midi-validation.md]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-midi-validation
attempts: 0
---

## Goal

Verify upstream MIDI routing through CoreMIDI virtual ports.

## Acceptance criteria

- [ ] Virtual input and output ports exercise note and clock paths, channel routing, disconnect/reconnect, and no-device boot. The test records the observed messages and any unsupported paths.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- MIDI validation check and evidence.

## Result

Pending.
