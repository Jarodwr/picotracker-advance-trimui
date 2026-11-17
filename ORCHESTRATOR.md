# Coordinator operating instructions

Read [the design](planning/orchestrator-design.md) and use `orchestrator.py` for dependency validation, work selection, job transitions and worker briefs. The coordinator owns job status, board cards and run history. Workers own only their assigned output files and isolated code worktrees.

Set scheduling parameters in [planning/orchestrator-config.json](planning/orchestrator-config.json). The CLI currently supports the documented priority order, downstream-count tie break, and maximum parallel jobs; additional policy choices in the design are reserved for later implementation.

## Operating loop

1. Run `python3 orchestrator.py validate`. Resolve broken references or cycles before dispatch.
2. Run `python3 orchestrator.py plan --max-parallel 2`. The output is ordered by priority, downstream reach and ID, with write-scope conflicts excluded.
3. For each selected job, run `python3 orchestrator.py brief JOB_ID` and assign that brief to one worker. For concurrent code edits, create a separate Git worktree and branch per job. The wrapper repository is initialized; keep the pinned upstream submodule unchanged in each worktree.
4. Move selected jobs to `queued`, then `running` with `transition JOB_ID STATUS --reason '...'`. Keep the visible board current.
5. Require workers to report evidence and changed files. Review acceptance criteria; move to `review`, then `done` only after accepting the result. Use `blocked` or return to `ready` with a recorded reason when needed.
6. Re-run `plan` after each accepted job. Do not start dependent jobs early. Record new dependencies in job files and validate again.

Only the coordinator edits board files. Preserve all `##` column headings exactly; `planning/board-headings.json` records each board's current schema and the CLI refuses a card move when the schema differs. The coordinator must not change that schema without an explicit user request.

The CLI produces plans and worker briefs; it does not launch agents. The coordinating agent uses the available agent tools to dispatch and review work. This separation keeps job selection inspectable and lets the same files work with different agent runtimes.

## Commands

```sh
python3 orchestrator.py validate
python3 orchestrator.py plan --max-parallel 2
python3 orchestrator.py brief pt-source-baseline
python3 orchestrator.py transition pt-source-baseline queued --reason 'Assigned to worker'
python3 orchestrator.py transition pt-source-baseline running --reason 'Worker started'
```
