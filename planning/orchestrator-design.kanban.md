# Orchestrator design

## To Do

## In Progress

## Review

## Blocked

## Done

#### Define the job model and lifecycle
<!-- id: orch-job-model -->
Specify a small job schema with stable ID, objective, inputs, outputs, acceptance criteria, status, dependencies, priority, estimates, owner/agent, and links to evidence. Define allowed status transitions and when a job is complete.
<!-- priority: critical -->
<!-- workload: hard -->

#### Specify dependency semantics
<!-- id: orch-dependencies -->
Define prerequisite edges, ready-job rules, cycle detection, failure/block propagation, and how optional dependencies differ from required ones. Require explicit dependencies when creating or splitting jobs.
<!-- priority: critical -->
<!-- workload: hard -->

#### Define work selection and creation policies
<!-- id: orch-policies -->
Choose configurable policies for selecting ready work and creating or splitting jobs. Include priority, dependency readiness, scope limits, acceptance criteria, and a human review gate for changes that expand project scope.
<!-- priority: high -->
<!-- workload: normal -->

#### Define parallel scheduling and agent parameters
<!-- id: orch-parallelism -->
Specify maximum concurrent agents, eligible parallel jobs, resource/conflict constraints, task isolation, and when to serialize work. Jobs that edit shared files or depend on one another must not run concurrently.
<!-- priority: high -->
<!-- workload: normal -->

#### Design failure, retry, and escalation behavior
<!-- id: orch-recovery -->
Define timeout and retry limits, how agents report blockers and partial results, when a failed job returns to ready, and which decisions or repeated failures require human input.
<!-- priority: high -->
<!-- workload: normal -->

#### Choose the Markdown representation and review flow
<!-- id: orch-storage -->
Design a Git-friendly format for jobs, dependency links, execution history, and board views. Keep Markdown as the source of truth and define how orchestrator updates remain visible and reviewable.
<!-- priority: high -->
<!-- workload: normal -->

#### Write the orchestrator design and example schedule
<!-- id: orch-design-doc -->
Document architecture, policy parameters and defaults, dependency graph rules, scheduling loop, job templates, failure handling, and a worked schedule using the picoTracker investigation tasks.
<!-- priority: critical -->
<!-- workload: hard -->
