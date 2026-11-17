---
id: s4-runtime-contracts
title: Define host audio and MIDI runtime contracts
status: done
priority: critical
estimate: M
depends_on: [s3-boot, s3-compatibility]
write_scope: [docs/adr/0013-macos-audio-midi-runtime.md, planning/findings/sprint-4-runtime-contracts.md]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-runtime-contracts
attempts: 1
---

## Goal

Define the wrapper-owned macOS audio and MIDI runtime contracts against pinned v3.2.1.

## Acceptance criteria

- [x] Trace upstream mixer timing and buffer formats, MIDI input/output registration and transport, device lifecycle, and thread ownership. Record a concrete SDL audio and CoreMIDI design with silent/no-device fallback and validation seams.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- ADR 0013 and a source-linked runtime contract finding.

## Result

ADR 0013 defines the wrapper compatibility layer, macOS backend composition, later Linux/Windows unsupported stubs, and single-thread firmware dispatch. The source-linked finding records 44.1 kHz stereo PCM, AudioOutDriver reuse, the two-entry audio pool, MIDI device slot limits, saved-setting indexing risk, and CoreMIDI callback queueing. SDL and Apple API contracts were checked against their official documentation. Implementation and runtime validation remain separate jobs.
