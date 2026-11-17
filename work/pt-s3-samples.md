---
id: s3-samples
title: Implement a bounded host sample pool
status: done
priority: high
estimate: L
depends_on: [s3-filesystem]
write_scope: [platform/desktop/HostSamplePool.h, platform/desktop/HostSamplePool.cpp]
owner: unassigned
board: planning/sprint-3-application-boot.kanban.md
board_card_id: s3-samples
attempts: 1
---

## Goal

Implement a bounded host sample pool for the pinned public Advance source without editing the submodule.

## Acceptance criteria

- [x] Sample loading uses configurable bounded host memory, fails predictably at capacity, and leaves upstream data formats unchanged.

## Inputs

- Sprint 2 findings, ADRs 0004–0006, and the pinned upstream interfaces.

## Output

- platform/desktop/HostSamplePool.h, platform/desktop/HostSamplePool.cpp

## Result

The host pool loads upstream WAV through `WavFile`, defaults to 128 MiB, and accepts 1–2047 MiB from the CLI. With a 1 MiB limit, a 200,000-byte PCM WAV loaded and a 1,000,000-byte WAV was rejected after the first occupied memory; remaining space stayed at 648,576 bytes. No upstream format change. See ADR 0010.
