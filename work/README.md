# Orchestrator jobs

Each job is one Markdown file. Its YAML frontmatter is the scheduler-facing record; the rest of the file contains goal, criteria, inputs, output, and result evidence. Job IDs are stable and dependency references use IDs.

Workers own their assigned job record. Concurrent code jobs also receive separate Git worktrees and branches. The coordinator alone updates the Kanban board and run log, then reviews worker evidence before setting a job to `done`.

The examples below form the first picoTracker investigation dependency graph. They are planning records, not dispatched work yet.

Create a new job from [job.template.md](job.template.md). Keep its `id` stable and set `depends_on` to existing job IDs. A job cannot be dispatched until its goal, acceptance criteria, expected output, and write scope are concrete.
