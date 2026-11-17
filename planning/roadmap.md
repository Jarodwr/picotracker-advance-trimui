# Roadmap after the application boot

The pinned Advance source remains a submodule. These milestones describe the wrapper project; detailed jobs are created when the preceding milestone supplies enough evidence to scope them.

1. **Sprint 4 — macOS audio, MIDI, and local bundle.** Finish real audio output, CoreMIDI routing, validation, and a runnable local app bundle. The detailed [Sprint 4 board](sprint-4-macos-audio-midi.kanban.md) is ready.
2. **Sprint 5 — macOS feature completeness.** Scope and implement the remaining user-facing gaps after Sprint 4, starting with device settings, and compatibility with any newer Advance project fixtures available for testing.
3. **Sprint 6 — macOS release readiness.** Test upgrades and data preservation, prepare distribution packaging and notices, and document installation and recovery. The local bundle from Sprint 4 is the starting point.
4. **Sprint 7 — Linux and Windows.** Revisit the deferred [portability job](../work/pt-s3-portability.md), add separate backend entries with explicit unsupported stubs, then port the proven host adapters and packaging approach to each platform. Verify builds and project compatibility on actual runners before claiming support.

Sprint 5–7 are roadmap milestones, not active boards. Their job files and dependencies will be based on the accepted results of the earlier sprints.

## Trimui Smart Pro (NextUI)

Separate from the macOS sprints above. Done: cross toolchain (ADR 0015), input and display (ADR 0016), package layout and CI zip (ADR 0017), audio capture adapter (ADR 0018), microphone recording (mono from the built-in mic; [findings](findings/trimui-recording.md); headset and real buttons unverified), emulated smoke. Open: a first run on the device (the app has not been run on hardware; see [the first-run checklist](../docs/trimui-first-run.md)), then the fixes it finds. Out of scope for the first release: firmware other than NextUI, and USB MIDI (deferred to a later board).
