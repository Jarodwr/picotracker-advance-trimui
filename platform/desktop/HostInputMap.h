/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ADR 0016 sections 1-3: joystick source -> Advance pad button mapping, the
// Select+Start+L1+R1 exit chord and the optional left-stick-as-D-pad state
// machine. Pure C++: no SDL headers, so the SDL joystick adapter only has to
// translate its events into InputSource values.

#include "UIFramework/BasicDatas/GUIEvent.h"

#include <array>
#include <cstdint>
#include <map>
#include <vector>

// Trimui controls by symbolic name (ADR 0016 section 1). Face buttons follow
// the physical position: East is the right button, South the bottom one.
enum class TrimuiControl {
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
  East,
  South,
  West,
  North,
  L1,
  R1,
  L2,
  R2,
  Start,
  Select,
  Menu,
  L3,
  R3,
  Count
};

struct InputSource {
  enum class Kind { Button, Hat, Axis };
  // Hat: direction is one of the Hat* values. Axis: -1 or +1 (stick side).
  enum Direction { HatUp = 1, HatDown = 2, HatLeft = 3, HatRight = 4 };

  Kind kind = Kind::Button;
  int index = 0;
  int direction = 0;

  static InputSource Button(int index) { return {Kind::Button, index, 0}; }
  static InputSource Hat(int index, Direction d) {
    return {Kind::Hat, index, d};
  }
  static InputSource Axis(int index, int sign) {
    return {Kind::Axis, index, sign < 0 ? -1 : 1};
  }
  bool operator<(const InputSource &o) const {
    if (kind != o.kind) return kind < o.kind;
    if (index != o.index) return index < o.index;
    return direction < o.direction;
  }
  bool operator==(const InputSource &o) const {
    return kind == o.kind && index == o.index && direction == o.direction;
  }
};

struct HostInputAction {
  // Quit is the unconditional hard exit (chord, no dialog); QuitRequest asks
  // the host to open the confirmation dialog (Menu, or Select+Start held).
  enum class Type { PadPress, PadRelease, Quit, QuitRequest };
  Type type = Type::Quit;
  GUIEventPadButtonType button = EPBT_INVALID; // pad types only

  bool operator==(const HostInputAction &o) const {
    return type == o.type && button == o.button;
  }
};

class HostInputMap {
public:
  // The default table uses SDL_GameController-style numbering (hat 0 for the
  // D-pad, buttons A=0 B=1 X=2 Y=3 Select=4 Menu=5 Start=6 L3=7 R3=8 L1=9
  // R1=10, axes 0/1 left stick). These indices are unverified on the device
  // (ADR 0016); the profile overrides them with Bind/Unbind.
  HostInputMap();

  static constexpr int kLeftStickX = 0;
  static constexpr int kLeftStickY = 1;
  // Select + Start held together this long asks for the quit dialog (C-60).
  static constexpr uint32_t kComboHoldMs = 1000;
  static constexpr float kStickPress = 0.50f;
  static constexpr float kStickRelease = 0.35f;

  void Bind(const InputSource &source, TrimuiControl control);
  void Unbind(const InputSource &source);
  // Turning the flag off releases any stick-held D-pad direction.
  std::vector<HostInputAction> SetLeftStickAsDpad(bool enabled);
  bool LeftStickAsDpad() const { return leftStickAsDpad_; }

  // Pad button for a bound source, or EPBT_INVALID. Controls without an
  // Advance equivalent (X, Y, L2, R2, Menu, L3, R3) resolve to EPBT_INVALID.
  GUIEventPadButtonType ResolvePadButton(const InputSource &source) const;
  static GUIEventPadButtonType PadButtonFor(TrimuiControl control);

  // Digital source (button or hat direction) changed state.
  std::vector<HostInputAction> Feed(const InputSource &source, bool pressed);
  // Analog axis moved; value is normalised to [-1, 1]. Only the left stick
  // does anything, and only with left_stick_as_dpad on.
  std::vector<HostInputAction> FeedAxis(int axis, float value);
  // Call regularly with a millisecond clock: detects the Select+Start hold.
  // The two pad releases keep the firmware free of stuck buttons, then
  // QuitRequest. Latches until both are released.
  std::vector<HostInputAction> Tick(uint32_t nowMs);
  // Device lost or window unfocused: release everything delivered.
  std::vector<HostInputAction> ReleaseAll();

private:
  static constexpr int kControls = static_cast<int>(TrimuiControl::Count);
  static bool InChord(TrimuiControl c);
  void Apply(const InputSource &origin, TrimuiControl control, bool pressed,
             std::vector<HostInputAction> &out);
  bool ChordHeld() const;

  std::map<InputSource, TrimuiControl> bindings_;
  std::map<InputSource, TrimuiControl> held_; // origin -> control it pressed
  std::array<int, kControls> heldCount_{};
  std::array<bool, kControls> delivered_{}; // a PadPress was forwarded
  bool chordLatched_ = false;
  bool comboLatched_ = false;
  bool comboTiming_ = false;
  uint32_t comboStartMs_ = 0;
  bool leftStickAsDpad_ = false;
  // Per-axis state: -1, 0 or +1 for the direction currently pressed.
  int stickState_[2] = {0, 0};
};
