# ADR 0002: Prove native compilation before wiring the full UI

Status: Accepted (2026-11-15)

## Context

The Advance firmware CMake target is STM32-specific. Direct Clang compilation with `ADV` succeeds for seven shared model translation units, but `Config.cpp` fails because `MAX_THEME_EXPORT_PATH_LENGTH` uses `strlen()` in a template argument in `PersistenceConstants.h`. The upstream compiler accepts this, while Apple Clang does not treat it as a constant expression.

## Decision

Build a wrapper-owned static library from the seven native-compiling model sources. Add an SDL2 host executable for window/input bring-up. Do not call the SDL scaffold an upstream UI port until it creates the real application and renders its views. Record each additional compiler failure and prefer a small upstream fix for constant-expression and interface issues.

## Consequences

The host build gives a repeatable compiler and SDL smoke check while the real application integration remains incomplete. Two file-scoped compiler compatibility definitions currently allow the complete shared model library to compile; they are temporary and must be replaced with narrow source fixes before release. The SDL scaffold implements `I_GUIWindowImp` drawing calls, uses the pinned Advance bitmap font, and calls upstream scale logic; it does not yet initialize `Application`. The upstream submodule stays clean. SDL2 is supplied by the host package manager for now; packaging and Windows/Linux dependency setup remain future work.
