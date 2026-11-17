# ADR 0007: Rebase desktop work on the dedicated Advance repository

Status: Accepted (2026-11-15)

## Context

The initial wrapper pins `xiphonics/picoTracker` v2.3.1 because it contains an Advance adapter. Xiphonics now publishes `xiphonics/advance-firmware` with stable source snapshots through v3.2.1, including the newer Advance instruments. This supersedes the earlier assumption that post-split Advance source was unavailable. Its licensing document identifies GPL-3.0-only new contributions, BSD inherited material, and other third-party terms.

## Decision

Treat the dedicated repository as the target upstream for the desktop port. Pin its `v3.2.1` snapshot at `upstream/advance-firmware` and adapt the wrapper-owned host build. Preserve the old source investigation as historical evidence.

## Consequences

The SDL scaffold and the `AppWindow`/`RecordView` compile probe build against v3.2.1. They do not boot the actual application. New host services must use the updated upstream interfaces, including the device identity seed. Distribution will require a complete license review of the combined desktop binary and bundled assets; `LICENSING.md` explicitly says its inventory is not a complete compatibility audit. New Sprint 3 implementation jobs depend on this source migration.
