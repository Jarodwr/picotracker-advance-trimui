# Sprint 1: Advance source and portability investigation

## To Do

## In Progress

## Blocked

## Review

## Done

#### Identify the public Advance source baseline
<!-- id: s1-source-baseline -->
Compare upstream branches and tags with current Advance firmware and its documented features. Record the exact commit, available source, missing features, and evidence; do not assume feature parity.
<!-- priority: critical -->
<!-- workload: hard -->

#### Review licenses and dependencies
<!-- id: s1-licenses -->
Inventory licenses for the selected source, third-party libraries, fonts and assets; note attribution and redistribution requirements and any missing dependencies.
<!-- priority: high -->
<!-- workload: normal -->

#### Investigate sample memory and project compatibility
<!-- id: s1-memory -->
Trace allocation, sample loading, address and file-format limits. Identify what would be required to exceed the device limit and how to detect projects that may not fit on an Advance.
<!-- priority: high -->
<!-- workload: hard -->

#### Map the tracker and platform boundaries
<!-- id: s1-platform-map -->
Identify reusable Advance UI, sequencing and audio code, build targets, and hardware dependencies for display, input, audio, storage, timing and MIDI. Record entry points and candidate adapters.
<!-- priority: critical -->
<!-- workload: hard -->

#### Select a desktop prototype approach
<!-- id: s1-prototype-plan -->
Use the evidence above to propose a host framework, source selection, build strategy, smallest practical proof of concept and known gaps. Identify changes needing upstream support or a maintained patch.
<!-- priority: high -->
<!-- workload: normal -->

#### Record the investigation and sprint 2 decision
<!-- id: s1-decision -->
Write findings with source links, exact revision, tested assumptions, risks and a go/no-go recommendation. Adjust sprint 2 scope to match the evidence.
<!-- priority: high -->
<!-- workload: normal -->
