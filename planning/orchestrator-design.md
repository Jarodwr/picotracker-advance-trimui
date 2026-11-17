# Orchestrator design proposal

## Purpose

The orchestrator turns a project backlog into a controlled set of agent jobs. It owns scheduling and coordination, not implementation quality: each worker is responsible for the assigned result, while the orchestrator checks the stated acceptance criteria, records evidence, updates dependencies and reports decisions that need a person.

For the first use, the orchestrator should manage the picoTracker Advance investigation and prototype. It should be useful with one worker as well as with several; parallelism is an optimization, not a requirement.

## Design principles

- Keep the source of truth in ordinary files tracked by Git.
- Require explicit job outputs and acceptance criteria before dispatch.
- Represent prerequisites as a directed acyclic graph (DAG); never dispatch work with unfinished required prerequisites.
- Prefer small independent jobs with one clear owner and bounded file scope.
- Let policy values be configured, but make the selected values visible in each run.
- Have workers return evidence and changed-file lists, not just a completion claim.
- Keep a person in control of scope changes, ambiguous product decisions, and consequential external actions.
- Treat Kanban column headings as a fixed schema: never rename, add, remove, duplicate, or reorder headings on an existing board. Move cards only between established headings. A schema change requires an explicit user request and a deliberate migration.

## Components

1. **Backlog**: Markdown job records and dependency links.
2. **Planner**: checks job quality, missing dependencies, cycles, and acceptance criteria; proposes task breakdowns when a job is too broad.
3. **Scheduler**: selects ready jobs under policy and resource constraints.
4. **Workers**: agents assigned one job each, with an isolated worktree when they may edit files.
5. **Reviewer**: verifies the result against acceptance criteria and evidence, then recommends `done`, `needs-revision`, or `blocked`.
6. **Run log**: append-only Markdown record of dispatches, outcomes, retries, decisions, and links to diffs.

The first implementation can combine planner, scheduler and reviewer in one coordinator agent. Keeping the responsibilities distinct in the design makes behavior testable and permits later replacement.

## Job record

Store each job as one Markdown file under `work/`. This is the selected source of truth: agents can own separate job records, the scheduler can inspect dependencies without parsing a large mixed board, and Git gives each job a readable history. The board is a view and planning aid; it is not a second place where job status is independently maintained. A small YAML frontmatter block carries fields used for scheduling; the body gives human-readable context.

```yaml
---
id: pt-platform-map
title: Map tracker and platform boundaries
status: ready
priority: high
estimate: M
depends_on: [pt-source-baseline]
parallel_group: source-review
write_scope: [planning/findings/platform-map.md]
owner: unassigned
attempts: 0
---

## Goal
Identify reusable Advance code and hardware-specific interfaces.

## Acceptance criteria
- Entry points and source paths are cited at the pinned revision.
- Display, input, audio, storage, timing and MIDI dependencies are listed.
- Candidate host adapter boundaries and unknowns are recorded.

## Inputs
- Upstream source baseline job output.

## Output
- `planning/findings/platform-map.md`
```

Valid statuses: `draft`, `ready`, `queued`, `running`, `review`, `blocked`, `done`, `cancelled`. `done` means a reviewer accepted the outputs; it does not merely mean the worker stopped.

Allowed transitions are `draft → ready`, `ready → queued`, `queued → running`, `running → review`, and `review → done`. The coordinator may move `queued → ready` when dispatch is withdrawn; `review → ready` for revision; or any unfinished job to `blocked` when a prerequisite or external condition prevents progress. A resolved blocker returns to `ready` after dependency validation. Cancellation is explicit and terminal; reopening a `done` or `cancelled` job requires a new job that cites the original. Every transition records its reason and actor in the run log.

Required fields for dispatch: stable ID, title, goal, acceptance criteria, expected output, status, priority, dependency list (empty is explicit), and write scope. Estimate and parallel group are useful but can be omitted initially. Unknown estimates do not block scheduling.

## Dependency rules

- `depends_on` lists required prerequisite job IDs. Every ID must exist.
- A job is **ready** only when it has all required fields, every required dependency is `done`, and it is not cancelled or blocked.
- Detect cycles before every scheduling pass. A cycle is a planning error: pause scheduling and report the cycle for repair.
- If a required prerequisite becomes blocked or cancelled, mark downstream jobs `blocked` with the reason. Do not silently drop the edge.
- Optional context belongs under `Inputs` or `related`, not in `depends_on`.
- Jobs that can start before an upstream result but need a later integration step should be split into independent work plus an explicit integration job.
- A worker may suggest a new dependency, but only the planner applies it after checking for cycles and documenting the reason.

## Policy parameters and initial defaults

| Parameter | Initial default | Effect |
| --- | --- | --- |
| `max_parallel` | 2 | Maximum concurrent worker jobs; set to 1 for serial mode. |
| `selection` | `priority_then_critical_path` | Select higher priority first, then work that unblocks the most downstream work, then stable ID order. |
| `priority_order` | critical, high, medium, low | Tie-break order for ready jobs. |
| `max_job_size` | M | Ask planner to split larger jobs before dispatch. |
| `max_retries` | 1 | One retry for transient failures after the cause is recorded. |
| `require_acceptance_criteria` | true | Incomplete jobs cannot be dispatched. |
| `require_write_scope` | true | Editing jobs state expected files/directories; unexpected changes go to review. |
| `isolation` | worktree for concurrent edits | Avoid collisions between workers; read-only jobs can share a checkout. |
| `human_gate_scope_change` | true | Ask a person before adding a new project objective or materially expanding scope. |
| `human_gate_external_action` | true | Ask before publishing, deploying, sending messages, or other external side effects. |

These are configuration values, not hidden prompt behavior. Each run records effective values in `runs/<run-id>.md`.

## Work creation and splitting

The planner can create jobs from an approved project objective or split an existing job when it has multiple independent outputs, crosses the size limit, or cannot state one testable completion condition. A split must preserve the parent objective and produce:

- child goals, outputs, acceptance criteria and write scopes;
- explicit child dependencies;
- a parent integration/review job depending on all required children;
- a note explaining why the split improves sequencing or parallel work.

The planner may create routine implementation subtasks within the approved objective. It must seek human direction before introducing new deliverables, platform targets, product behavior or external commitments. Uncertain details should become a bounded investigation job rather than an assumed requirement.

## Scheduling and parallel work

At each scheduling pass:

1. Parse jobs and validate IDs, fields, dependency references and acyclic graph.
2. Reconcile statuses with the run log; flag stale `running` jobs for review.
3. Find `ready` jobs whose dependencies are all `done`.
4. Sort by configured selection policy. For critical-path tie-breaking, count unfinished downstream jobs; larger counts go first.
5. Admit jobs up to `max_parallel`, subject to conflicts.
6. Dispatch each job with its goal, criteria, inputs, allowed write scope, expected report format and instruction to stop if scope is insufficient.

Two jobs are eligible to run together only if neither depends on the other, their write scopes do not overlap, and they do not contend for a scarce shared resource (for example, one GUI session). If conflict cannot be determined, serialize them. Read-only research may run in parallel when its source/revision is fixed.

Each concurrently running code job gets its own Git worktree and branch. Different job Markdown files reduce conflicts in the task records, while worktrees isolate source changes, generated build files and commits. Worktrees do not make overlapping edits safe by themselves: the scheduler still compares declared write scopes and serializes conflicts. An integration job consumes worker branches and resolves cross-job changes in a designated worktree. Avoid having workers update the same Kanban board during execution; the coordinator batches board status updates.

If several agents are unavailable, the same queue can be worked serially. A job is never considered parallelizable merely because it has a different title.

## Worker result contract

Each worker returns:

- job ID and outcome (`completed`, `partial`, `blocked`, `failed`);
- summary of work and acceptance-criteria checklist;
- evidence: file paths, source references, commands/output where relevant;
- changed files and a diff/worktree reference;
- new unknowns, risks and suggested follow-up jobs;
- whether any write exceeded the declared scope.

Workers do not mark jobs `done`. They submit results for review. They must not change the dependency graph or claim work they could not verify.

## Review, failure and recovery

- Review compares each criterion with concrete evidence and checks the changed-file list against write scope.
- Accepted output moves to `done`; incomplete but salvageable output returns to `ready` with precise feedback; an external blocker moves to `blocked`.
- A transient failure can be retried up to `max_retries`, preserving the attempt history. Repeated failure is escalated rather than retried indefinitely.
- A failed worker does not imply the job itself is impossible. Record the failure cause and decide whether to retry, split, reassign or block.
- On worker timeout or coordinator restart, inspect worktree and run log before redispatching to avoid duplicated or overwritten work.
- Downstream jobs remain blocked until prerequisites are accepted as `done`.
- Human input is requested for unclear product decisions, scope growth, unresolved dependency conflicts and actions designated by the human-gate settings.

## Storage and visibility

Proposed layout:

```text
planning/
  orchestrator-design.md
  sprint-1-investigation.kanban.md
  sprint-2-desktop-prototype.kanban.md
  orchestrator-design.kanban.md
work/
  README.md
  pt-source-baseline.md
  pt-platform-map.md
  pt-license-review.md
  pt-memory-limits.md
  pt-prototype-plan.md
  pt-investigation-decision.md
runs/
  README.md
  2026-11-15-initial-plan.md
```

The coordinator updates the job frontmatter and appends outcome/evidence to the job body. Board cards link to job IDs/files and show coarse phase status. If the board duplicates per-job status, the coordinator is the only writer and reconciles it from job files; workers update only their assigned job and isolated worktree.

MD Kanban remains the visual phase/sprint board. Job files carry dependency and dispatch detail that the board format does not express well. Board cards link to job files. The run log makes dispatch and status changes auditable. Every state change is saved to disk promptly so VS Code's MD Kanban file watcher can refresh the open board; concurrent edits to the same board are avoided.

### Fixed board schema

Use [the static board template](board.template.md) when creating a new phase board. Keep these headings in this order:

```markdown
# <Board title>

## To Do

## In Progress

## Blocked

## Review

## Done
```

For an existing board, preserve its headings and order exactly, even if they differ from the template. The orchestrator must never change a heading while moving cards. Validate heading uniqueness and compare them with the board's saved schema before writing. If a needed state has no column, record it in the job file/run log and ask the user before changing the board schema. This rule would have prevented the duplicate `Review` heading.

### Job issue template

Copy [the job template](../work/job.template.md) to `work/<job-id>.md`, then replace placeholders and validate dependencies before marking it `ready`.

```markdown
---
id: <stable-kebab-case-id>
title: <short action-oriented title>
status: draft
priority: medium
estimate: M
depends_on: []
write_scope: []
owner: unassigned
attempts: 0
---

## Goal
<One concrete outcome.>

## Acceptance criteria
- [ ] <Observable condition for acceptance.>

## Inputs
- <Links to prerequisite job IDs or relevant sources.>

## Output
- <Expected file, decision, artifact, or other result.>

## Result
<Outcome, evidence, changed files, unknowns, and follow-up suggestions.>
```

New jobs start as `draft`. The planner fills in dependencies, write scope and criteria, validates the graph, then changes the record to `ready` before scheduling. The coordinator alone mirrors coarse job status to a board card.

## Example: picoTracker investigation schedule

Assume `max_parallel=2` and the policies above.

```text
pt-source-baseline ─┬─> pt-platform-map ──────┐
                    ├─> pt-license-review ────┼─> pt-prototype-plan ─> pt-investigation-decision
                    └─> pt-memory-limits ─────┘
```

1. `pt-source-baseline` starts first because source/revision selection gates all source claims.
2. Once accepted, platform mapping, license review and memory investigation are ready. The scheduler runs two at once (platform mapping and memory limits, for example) and queues license review next, respecting the cap and conflict rules.
3. `pt-prototype-plan` waits for all three analyses. It selects a minimal host proof based on actual available source and constraints.
4. `pt-investigation-decision` depends on the plan and records the go/no-go decision and concrete sprint 2 scope.

No worker should start modifying upstream code during this investigation. If the source review reveals that a runnable prototype cannot be scoped responsibly, the decision job records that and sprint 2 is revised before implementation.

## Rollout

1. Use this design manually: create explicit jobs and dependencies for the investigation, dispatch at most two independent jobs, and maintain the run log.
2. Evaluate whether Markdown job files and MD Kanban provide enough visibility and whether status updates remain reliable.
3. Prototype scheduling as a coordinator workflow over the files; validate dependency parsing and cycle detection before allowing automatic dispatch.
4. Add parallel dispatch only after write-scope and worktree handling are dependable.

The first useful orchestrator is a repeatable coordination protocol, not a large autonomous service. Automate only the steps whose inputs and outcomes are explicit.
