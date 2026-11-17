# Sprint 3 application boot result

Source: `xiphonics/advance-firmware` v3.2.1, pinned at `0282da7e50cef0bdc88e4084ae667dfd6ff55f1a`.

The macOS wrapper now calls `Application::Init()` and renders the actual Song view. The first view was captured in `build/first-firmware-view.png` during local verification. `--smoke` dispatches SDL Down through `HostEventManager` into `AppWindow`; the renderer hash changed from `3f508aa57917d03b` to `dad09b6b5d16d03b` on the test run. The smoke exit status was zero. This tests real upstream view navigation, not only the earlier GUI probe.

On first run, the firmware created its config and untitled project under the specified `--data-dir`. On the next run, it loaded that project successfully. The silent audio service starts and no host audio device is opened.

The host build still uses compatibility compiler definitions and wrapper stubs for firmware-only symbols. Audio is silent and hardware MIDI is unavailable. Linux and Windows builds are tracked separately.
