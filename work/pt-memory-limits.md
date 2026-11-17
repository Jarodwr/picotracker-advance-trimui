---
id: pt-memory-limits
title: Investigate sample memory and project compatibility
status: done
priority: high
estimate: M
depends_on: [pt-source-baseline]
write_scope: [planning/findings/memory-limits.md]
owner: unassigned
board: planning/sprint-1-investigation.kanban.md
board_card_id: s1-memory
attempts: 1
---

## Goal
Trace sample allocation and project loading limits, distinguishing hardware memory limits from fixed implementation and file-format limits.

## Acceptance criteria
- Cite allocations, buffers, sample-size fields and relevant format constraints.
- Explain which limits are host-specific and which affect Advance compatibility.
- Recommend how a desktop build should handle projects exceeding device limits.

## Inputs
- `pt-source-baseline` output.

## Output
- `planning/findings/memory-limits.md`

## Result

Completed at pinned `v2.3.1`. The finding traces both SDRAM pools, per-sample fit, entry limits, WAV size types and the import/loader size mismatch, and recommends separate desktop and device-compatibility budgets. Changed file: `planning/findings/memory-limits.md`.
