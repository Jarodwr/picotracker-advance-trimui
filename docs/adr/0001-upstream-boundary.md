# ADR 0001: Keep firmware source pinned and add a host build outside it

Status: Accepted (2026-11-15)

## Context

The last tagged public source containing the Advance adapter is `v2.3.1`. Its existing CMake target builds for STM32H7 and FreeRTOS. The wrapper project aims to run reusable tracker code on macOS, Linux and Windows without changing the upstream checkout.

## Decision

Pin the official firmware as a Git submodule at `ce77d1f5b31f75a9b772384572ed6705e8c86e3d`. Put host CMake targets, platform adapters and compatibility checks in this wrapper repository. Begin by compiling a shared application model library natively; then add a host window, input, audio and storage behind existing service interfaces. A source change that cannot be kept outside upstream becomes a narrowly documented patch or an upstream contribution.

## Consequences

Upstream updates are deliberate. The host build may need to list upstream source files because the firmware CMake root chooses embedded targets. Desktop builds may support only the public `v2.3.1` feature set until later Advance source is available. The submodule working tree remains clean.
