# Sprint 1 decision: proceed with a public-source desktop prototype

**Decision:** Go for a bounded desktop UI/input prototype from the official `v2.3.1` Advance source. Do not promise current Advance feature parity or arbitrary sample memory yet.

The submodule is pinned to `ce77d1f5b31f75a9b772384572ed6705e8c86e3d`. The [source baseline](source-baseline.md) identifies the last tagged public Advance adapter and the later repository split. The [platform map](platform-map.md) identifies a service-installation seam for a host adapter. The [license review](license-review.md) identifies upstream, dependency and font notices. The [memory investigation](memory-limits.md) traces the 32+16 MiB pools and shared fixed-size arithmetic. The [prototype plan](prototype-plan.md) defines a first runnable UI/input slice.

## What this decision does and does not establish

- It establishes a reproducible public source revision with an Advance adapter and shared application code.
- It does not establish that the public revision implements current FM6, StarLoom or PicoSwarm features described by the current manual.
- It does not establish desktop buildability; sprint 2 must test the compile seam.
- It does not establish current-device project compatibility; version-matched fixtures and later cross-version checks are required.

## Sprint 2 scope

Build a wrapper-owned host target around the pinned submodule. First prove a shared library can compile natively, then draw an initial Advance-style view and process one key. Audio and project loading follow only if the first seam works. Keep any patches outside the upstream working tree and document them.

If the user requires current Advance feature parity, that work waits for an accessible authorized source for the later firmware. This does not block the public-source portability proof.
