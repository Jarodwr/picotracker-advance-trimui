# ADR 0016: Trimui Smart Pro input mapping and display scaling

Status: Accepted (2026-11-17), amended after implementation and the C-14 research report. The device-only items under "Unverified" remain unverified.

## Context

ADR 0009 translates SDL keyboard events into upstream `GUIEventPadButtonType` pad events in `platform/desktop/HostEvent.cpp`. The Advance firmware uses ten of them: D-pad `UP/DOWN/LEFT/RIGHT`, `A`, `B`, `L`, `R`, `START` and `SELECT`. `MapKey` is the only place the desktop host chooses between physical input and pad button.

The desktop display path draws into a fixed 720x720 square surface: `HostDisplay.cpp` clips at 720, `SDLWindow::DrawRect` scales the upstream 320x240 logical rect to 720x720, and `main.cpp` creates a 720x720 window. The Advance panel is therefore treated as a 720x720 square.

The Trimui Smart Pro (tg5040, aarch64, 1280x720 landscape panel, PowerVR GE8300) has D-pad, A/B/X/Y, L1/R1/L2/R2, Start/Select/Menu and two analog sticks (C-14 report, secondary sources). How the buttons and sticks are presented to apps (SDL joystick index, hat or buttons, stick axes) is **unverified** and needs the device. The Menu button is probably taken by the launcher.

## Decision

### 1. Button mapping (defaults)

The Trimui host adds a joystick source feeding the same `GUIEvent` pad path as the keyboard. The default table (symbolic Trimui names, resolved to SDL indices by the device profile in section 4):

| Advance pad button | Trimui control |
|---|---|
| `EPBT_UP/DOWN/LEFT/RIGHT` | D-pad |
| `EPBT_A` | A (east, right face button) |
| `EPBT_B` | B (south, bottom face button) |
| `EPBT_L` | L1 |
| `EPBT_R` | R1 |
| `EPBT_START` | Start |
| `EPBT_SELECT` | Select |

Notes: the Advance A/B sit in the same layout as GBA A/B, so Trimui face-button positions follow the Nintendo convention (A right, B bottom) and not the Xbox-style label. `X` and `Y` have no Advance equivalent and are unbound by default (available as editing helpers, see combos). Existing desktop keys in `MapKey` stay in the keyboard source, unchanged.

### 2. Missing hardware buttons

The Advance has no hardware that the Trimui lacks, so there is nothing to emulate by combo. The extras are used instead:

- **L2/R2** are unbound by default (reserved for user remapping).
- **Menu** is left to the launcher and treated as unavailable. The app must not depend on it.
- **Exit:** the desktop's `Escape` quit becomes the chord **Select + Start + L1 + R1** held together. This chord is evaluated in the input layer, never forwarded as pad events, and sets the existing `quit_` flag so `launch.sh` returns to the launcher. Rationale: the app needs a deterministic exit even if Menu is intercepted.
- Chords are a table of `{set of Trimui controls} -> host action`, so further host actions (for example "toggle debug overlay") can be added without touching pad mapping. Only the exit chord is defined now.

### 3. Sticks

Sticks are **ignored** by default. The Advance has no analog input, and mapping a stick to the D-pad would cause accidental cursor movement in the tracker grid. A profile flag `left_stick_as_dpad` (default false) may map the left stick to the D-pad with a 50 % dead zone and hysteresis (press at 50 %, release at 35 %), for users who prefer it. The right stick and stick clicks (L3/R3) are always ignored. The flag is the only stick feature in scope; analog values never reach upstream.

### 4. Configurable mapping

- Mapping is **data, not code**: `TrimuiInputProfile`, a table of `{source (button/hat/axis, SDL index), pad button}` plus the chord table and stick flag, loaded from `<data root>/input.ini` (data root per ADR 0008) with simple `ADVANCE_NAME = trimui_name` lines. A missing or malformed file falls back to the compiled default and logs which lines were rejected.
- The compiled defaults live in a device profile keyed by `SDL_JoystickName` (and `DEVICE=smartpro` from the launcher). Because the SDL indices are unknown, the profile is symbolic first (`SDL_GameController` names where SDL provides a mapping, raw joystick indices otherwise). On first run with `--log-input`, the host logs each joystick name, button/axis/hat counts and every press index, so the profile can be completed on the device.
- No in-app remapping UI in this ADR (follow-up).

### 5. Display scaling

The Advance framebuffer is 720x720 and the Trimui panel is 1280x720:

- **Scale: 1x integer.** The 720x720 frame is drawn unscaled, so there is no filtering decision and every pixel is exact. Generally: `scale = max(1, floor(min(panelW / 720, panelH / 720)))`; for 1280x720 that is 1.
- **Letterbox:** the frame is centered with black borders of 280 px left and right (`x = (panelW - 720 * scale) / 2`, `y = (panelH - 720 * scale) / 2`). Other panel sizes (desktop window resized, Brick 1024x768) use the same formula; if the panel is smaller than 720 on an axis, scale falls back to a fractional fit.
- **Rotation: none.** The panel is landscape and the Advance image is upright, so no rotation is applied. A profile field `rotation` (0 only for now) is reserved.
- **Filter:** nearest neighbour (`SDL_HINT_RENDER_SCALE_QUALITY=0`); never linear, which would blur text.

### 6. Fit with SDLWindow / HostDisplay

Do not change the draw code that targets 720x720. The integer-scale step belongs after drawing, in the renderer setup:

- Create the renderer at the panel size and call `SDL_RenderSetLogicalSize(renderer, 720, 720)` with `SDL_RenderSetIntegerScale(renderer, SDL_TRUE)`. SDL then performs the centering, letterboxing and integer scaling, and `SDLWindow`/`HostDisplay` keep drawing in 720x720 coordinates unchanged.
- Window creation takes its size from a `HostDisplayGeometry` (see below) instead of the literal 720 in `main.cpp`; on the Trimui it is fullscreen at the native 1280x720 mode.
- Use the software renderer (`SDL_RENDERER_SOFTWARE`) as the primary Trimui path: the GE8300 is PowerVR, not Mali, and GL/GLES availability is unknown (C-14). The existing accelerated-then-software fallback in `main.cpp` already handles this; the Trimui build should prefer software and skip the accelerated attempt.
- `SDL_RenderReadPixels` frame hashing in `main.cpp` reads the logical target and stays valid.

## Implementation outline and test seams

For the follow-up port card:

1. **`HostInputMap`** (new, pure C++, no SDL headers): `ResolvePadButton(source) -> GUIEventPadButtonType`, `Feed(source, pressed) -> list<HostInputAction>` where an action is a pad event or `Quit`. Holds the chord detector and stick dead-zone state machine. *Seam:* unit-tested with synthetic sources: every default mapping, Select+Start+L1+R1 yields `Quit` and no pad events, stick ignored when flag off, hysteresis when on.
2. **`TrimuiInputProfile` parser:** `Parse(text) -> {profile, rejected lines}`. *Seam:* tested on strings (valid, partial, malformed, unknown names).
3. **`SDLJoystickSource`:** thin adapter translating `SDL_JOYBUTTONDOWN/UP`, `SDL_JOYHATMOTION`, `SDL_JOYAXISMOTION` and device add/remove into `HostInputMap` sources, called from `HostEventManager::Handle`. Only this layer includes SDL; tested with hand-built `SDL_Event`s (as the existing key test does).
4. **`ComputeDisplayLayout(panelW, panelH) -> {scale, x, y}`** pure function. *Seam:* table tests: 1280x720 → scale 1, x 280, y 0; 1024x768 → 1, 152, 24; 720x720 → 1, 0, 0; 1440x1440 → 2; 640x480 → fractional fit.
5. **Renderer setup** in `main.cpp` chooses geometry from a `--display trimui|desktop` switch (or build-time backend per ADR 0012/0013), applying logical size and integer scale.
6. **Device checklist** for the first run: log joystick name and indices, confirm face-button positions and Menu interception, confirm the 280 px borders and exit chord.

## Amendments from the implementation

- **Options.** `--display trimui|desktop`, `--log-input` and `--input-profile PATH` exist in `platform/desktop/main.cpp`. Without `--input-profile` the profile is read from `<data root>/input.ini`. The shipped `launch.sh` passes `--display trimui` but not `--log-input`; add it by hand to log joystick names, button/axis/hat counts and every press index.
- **`input.ini` format** (`HostInputProfile`): one `advance_button = trimui_control` per line; `#` or `;` start a comment on its own line; a UTF-8 BOM is skipped. Advance names: `up down left right a b l r start select`. Controls: `dpad_up dpad_down dpad_left dpad_right east south west north l1 r1 l2 r2 start select menu l3 r3`, or `none` to unbind. `left_stick_as_dpad` takes `true/false/1/0/yes/no/on/off`. A line is rejected (and logged to stderr with its line number and reason) when it has no `=`, an empty name or value, an unknown name or control, a bad boolean, or repeats a key; the other lines still apply, and a missing file uses the defaults. The defaults are those in section 1, with `left_stick_as_dpad = false`; `packaging/trimui/share/input.ini.example` lists them.
- **Chord policy.** Chord members are forwarded as pad events as they arrive so that L and Start are not delayed. When the fourth member goes down the chord latches: releases are emitted for the members already delivered (no stuck buttons), then `Quit`; the last member is never forwarded, and the latch clears when all four are up. The chord is fixed and is not configurable in `input.ini`.
- **Display** is as decided: native fullscreen window, software renderer first, nearest-neighbour hint, `SDL_RenderSetLogicalSize(720, 720)` with integer scale.
- **Research report.** The report's (C-14) button names are secondary sources; the symbolic names above are resolved against SDL joystick indices by the profile, so a wrong guess is fixed in `input.ini` rather than in code.

## Amendment: input verified on the device (C-44)

Read-only probing of the Trimui Smart Pro (NextUI 20261013-0) over ssh, with the app's own SDL joystick code run headless (`--log-input`, dummy video and audio):

- `trimui_inputd` creates the pad through `/dev/uinput` as **'TRIMUI Player1'**, USB ids `045e:028e` (an Xbox 360 pad), on `/dev/input/event3` and `/dev/input/js0`. Key capabilities are `BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR`; absolute axes are `ABS_X/Y/Z/RX/RY/RZ` and `ABS_HAT0X/Y`. There are no `BTN_DPAD_*` keys and no `BTN_TL2/TR2`.
- SDL (our static 2.26.1 here) opens it as **'Xbox 360 Controller', buttons=11, axes=6, hats=1**. Button indices follow the key codes in order, so they are:

| SDL index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Control | south (B) | east (A) | west (X) | north (Y) | L1 | R1 | Select | Start | Menu | L3 | R3 |

  The D-pad is **hat 0**; the sticks are axes 0/1 (left) and 3/4 (right); **L2/R2 are analog axes 2 and 5**, not buttons. The earlier guess (Select 4, Menu 5, Start 6, L3/R3 7/8, L1/R1 9/10, L2/R2 as buttons 11/12) was wrong for those controls; the face buttons and the D-pad were right.
- The compiled defaults now use that table. L2/R2 have no button source: the profile can still name them, but they stay unbound. The exit chord is unchanged (Select + Start + L1 + R1 = buttons 6, 7, 4, 5).
- No grab: `/tmp/trimui_inputd/grab` does not exist while NextUI is idle, and `keymon.elf` only reads `event3` (no exclusive grab seen). Both keep running next to the app, so Menu combos handled by `keymon` can still fire while PicoTracker runs; the launcher is not a second copy of the events.
- Not verifiable without a person: which physical button emits which code. East = A and south = B follow the NextUI tg5040 platform (A=1, B=0, X=3, Y=2 in its joystick codes); the face-button positions and Menu interception need pressing. The test list is on card C-44.
- `--log-input` is implemented (joystick name, button/axis/hat counts, every press index to stderr).

## Consequences

- Desktop behaviour is unchanged; Trimui is an additional input source and a renderer configuration.
- Mapping indices were guesses until the C-44 amendment verified the SDL enumeration on the device; the physical button positions and the Menu behaviour still need a person, which is why the profile is data-driven and logs its input.
- 1x integer scale wastes 560 horizontal pixels but is crisp and free; a 720p-tall panel cannot fit 2x. Revisit if a taller panel is targeted.
- Ignoring sticks removes a source of accidental input but means no analog features; the `left_stick_as_dpad` flag is the escape hatch.
- Still unverified on hardware (the app has not been run on the device; the emulated smoke uses a virtual uinput gamepad and the SDL dummy drivers): the real `js0`/event button and stick indices of 'TRIMUI Player1' against the default table, Menu interception, KMS/DRM or fbdev output, the 1280x720 mode and the 280 px borders. From C-14: SDL joystick presentation of buttons/sticks, Menu interception, available video backend and 1280x720 mode.
