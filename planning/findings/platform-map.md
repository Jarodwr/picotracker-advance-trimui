# Platform boundaries at `v2.3.1`

The pinned build chooses Pico or Advance in [`sources/CMakeLists.txt`](../../upstream/picoTracker/sources/CMakeLists.txt) using `ADV`. It then compiles shared `Application`, `UIFramework`, `Services`, `System`, and `Foundation` libraries with a hardware adapter. There is no desktop target in this tag. The Advance branch depends on STM32H7, FreeRTOS, FatFs and TinyUSB; its [`CMakeLists.txt`](../../upstream/picoTracker/sources/Adapters/adv/CMakeLists.txt) produces an MCU firmware image.

## Startup and service seams

[`Core/Src/main.c`](../../upstream/picoTracker/sources/Adapters/adv/Core/Src/main.c) initializes hardware, calls `advSystem::Boot()`, initializes `Application`, and enters the event loop. [`advSystem::Boot()`](../../upstream/picoTracker/sources/Adapters/adv/system/advSystem.cpp) installs implementations of `System`, `I_GUIWindowFactory`, `TimerService`, `FileSystem`, `MidiService`, `Audio`, and `SamplePool`; installation order matters because audio reads configuration that uses MIDI. This factory/service boundary is the main route to a host adapter without editing the submodule.

| Concern | Shared interface / consumer | Advance implementation | Desktop candidate |
| --- | --- | --- | --- |
| Display and UI | `UIFramework`, application views, `I_GUIWindowFactory` | `gui/advGUIWindowImp.cpp`, `display/display.c` | SDL window/renderer using the existing UI draw calls and font data. |
| Input | `EventManager`, application commands | `gui/advEventManager.cpp`, `system/input.cpp` | SDL keyboard/gamepad mapping to existing button events. |
| Audio | `Services/Audio/Audio`, player/mixer | `audio/advAudio.cpp`, `advAudioDriver.cpp` | Host callback/device output with explicit 44.1 kHz buffering and underrun checks. |
| Samples | `SamplePool`, `WavFile` | `system/advSamplePool.cpp` | Desktop allocator plus Advance compatibility checker. |
| Files | `System/FileSystem/FileSystem` | `filesystem/advFileSystem.cpp`, FatFs | Host filesystem rooted in a user-selected data directory. |
| Timing | `System/Timer/Timer` | `timer/advTimer.cpp`, FreeRTOS | Monotonic host clock and timer scheduling. |
| MIDI | `Services/Midi/MidiService` | `midi/advMidiService.cpp` and device classes | Host MIDI API behind the same service; stub initially if the UI permits it. |
| Device services | `System/System/System` | `system/advSystem.cpp`, battery, power, USB | Host implementation with explicit unsupported-device behavior. |

## Suggested first host boundary

Start a separate CMake target in this wrapper project and compile selected shared source libraries against host implementations of the interfaces above. Do not compile STM32 startup/HAL or the Advance adapter into the host binary. Reuse the Advance screen dimensions, layout and font where the license permits, but expect conditional compilation (`ADV`) and hardware assumptions in shared code to need a small upstream extension or an external patch. Record each such case before changing it.

For the first runnable slice, initialize the app with a host `System`, window, event manager, timer and filesystem, then draw the initial UI and accept one navigation key. Add audio only after event/UI startup works. This sequence tests the central portability seam without pretending to emulate the device hardware.

## Unknowns to resolve in the prototype

- Whether every shared target can compile natively with host definitions and C++23; `ADV` branches may pull in STM32 headers.
- Whether the renderer exposes enough primitives to implement the 720×720 view exactly.
- Audio callback ownership, buffering and timing under a desktop scheduler.
- Project loading differences between this public source and current firmware.
