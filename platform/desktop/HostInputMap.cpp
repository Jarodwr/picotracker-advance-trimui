// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostInputMap.h"

namespace {

// Source indices are the xpad-style layout SDL reports for the tg5040 pad
// ('Xbox 360 Controller', 11 buttons, 6 axes, 1 hat); see ADR 0016.
constexpr TrimuiControl kChord[] = {TrimuiControl::Select, TrimuiControl::Start,
                                    TrimuiControl::L1, TrimuiControl::R1};

int Idx(TrimuiControl c) { return static_cast<int>(c); }

TrimuiControl StickControl(int axis, int sign) {
  if (axis == HostInputMap::kLeftStickX)
    return sign < 0 ? TrimuiControl::DpadLeft : TrimuiControl::DpadRight;
  return sign < 0 ? TrimuiControl::DpadUp : TrimuiControl::DpadDown;
}

} // namespace

HostInputMap::HostInputMap() {
  using D = InputSource::Direction;
  Bind(InputSource::Hat(0, D::HatUp), TrimuiControl::DpadUp);
  Bind(InputSource::Hat(0, D::HatDown), TrimuiControl::DpadDown);
  Bind(InputSource::Hat(0, D::HatLeft), TrimuiControl::DpadLeft);
  Bind(InputSource::Hat(0, D::HatRight), TrimuiControl::DpadRight);
  Bind(InputSource::Button(0), TrimuiControl::South);
  Bind(InputSource::Button(1), TrimuiControl::East);
  Bind(InputSource::Button(2), TrimuiControl::West);
  Bind(InputSource::Button(3), TrimuiControl::North);
  Bind(InputSource::Button(4), TrimuiControl::L1);
  Bind(InputSource::Button(5), TrimuiControl::R1);
  Bind(InputSource::Button(6), TrimuiControl::Select);
  Bind(InputSource::Button(7), TrimuiControl::Start);
  Bind(InputSource::Button(8), TrimuiControl::Menu);
  Bind(InputSource::Button(9), TrimuiControl::L3);
  Bind(InputSource::Button(10), TrimuiControl::R3);
  // L2/R2 are analog axes 2 and 5 on the device ('Xbox 360 Controller', 11
  // buttons): they have no button source and stay unbound.
}

void HostInputMap::Bind(const InputSource &source, TrimuiControl control) {
  if (source.kind == InputSource::Kind::Axis) return; // sticks: flag only
  bindings_[source] = control;
}

void HostInputMap::Unbind(const InputSource &source) { bindings_.erase(source); }

std::vector<HostInputAction> HostInputMap::SetLeftStickAsDpad(bool enabled) {
  std::vector<HostInputAction> out;
  leftStickAsDpad_ = enabled;
  if (!enabled) {
    for (int axis = 0; axis < 2; ++axis) {
      if (stickState_[axis] != 0)
        Apply(InputSource::Axis(axis, stickState_[axis]),
              StickControl(axis, stickState_[axis]), false, out);
      stickState_[axis] = 0;
    }
  }
  return out;
}

GUIEventPadButtonType
HostInputMap::PadButtonFor(TrimuiControl control) {
  switch (control) {
  case TrimuiControl::DpadUp: return EPBT_UP;
  case TrimuiControl::DpadDown: return EPBT_DOWN;
  case TrimuiControl::DpadLeft: return EPBT_LEFT;
  case TrimuiControl::DpadRight: return EPBT_RIGHT;
  case TrimuiControl::East: return EPBT_A;
  case TrimuiControl::South: return EPBT_B;
  case TrimuiControl::L1: return EPBT_L;
  case TrimuiControl::R1: return EPBT_R;
  case TrimuiControl::Start: return EPBT_START;
  case TrimuiControl::Select: return EPBT_SELECT;
  default: return EPBT_INVALID;
  }
}

GUIEventPadButtonType
HostInputMap::ResolvePadButton(const InputSource &source) const {
  if (source.kind == InputSource::Kind::Axis) {
    if (!leftStickAsDpad_ ||
        (source.index != kLeftStickX && source.index != kLeftStickY))
      return EPBT_INVALID;
    return PadButtonFor(StickControl(source.index, source.direction));
  }
  const auto it = bindings_.find(source);
  return it == bindings_.end() ? EPBT_INVALID : PadButtonFor(it->second);
}

bool HostInputMap::InChord(TrimuiControl c) {
  for (const auto m : kChord)
    if (m == c) return true;
  return false;
}

bool HostInputMap::ChordHeld() const {
  for (const auto m : kChord)
    if (heldCount_[Idx(m)] == 0) return false;
  return true;
}

// Chord policy: members are forwarded as ordinary pad events as they arrive
// (holding L or Start back would add latency to every use). When the fourth
// member goes down the chord latches: releases are emitted for the members
// already delivered, so the firmware sees no stuck buttons, then Quit. The
// last member is never forwarded. While latched, member presses and releases
// are swallowed; the latch clears once all four are up.
void HostInputMap::Apply(const InputSource &origin, TrimuiControl control,
                         bool pressed, std::vector<HostInputAction> &out) {
  const auto held = held_.find(origin);
  if (pressed) {
    if (held != held_.end()) return;
    held_[origin] = control;
    if (heldCount_[Idx(control)]++ != 0) return;
    if (InChord(control)) {
      if (chordLatched_) return;
      if (ChordHeld()) {
        chordLatched_ = true;
        for (const auto m : kChord) {
          if (!delivered_[Idx(m)]) continue;
          delivered_[Idx(m)] = false;
          out.push_back({HostInputAction::Type::PadRelease, PadButtonFor(m)});
        }
        out.push_back({HostInputAction::Type::Quit, EPBT_INVALID});
        return;
      }
    }
    if (control == TrimuiControl::Menu) {
      if (!chordLatched_)
        out.push_back({HostInputAction::Type::QuitRequest, EPBT_INVALID});
      return;
    }
    const auto button = PadButtonFor(control);
    if (button == EPBT_INVALID) return;
    delivered_[Idx(control)] = true;
    out.push_back({HostInputAction::Type::PadPress, button});
    return;
  }
  if (held == held_.end()) return;
  const auto released = held->second; // the control it pressed, even if rebound
  held_.erase(held);
  if (--heldCount_[Idx(released)] != 0) return;
  if (delivered_[Idx(released)]) {
    delivered_[Idx(released)] = false;
    out.push_back(
        {HostInputAction::Type::PadRelease, PadButtonFor(released)});
  }
  if (chordLatched_) {
    bool anyHeld = false;
    for (const auto m : kChord) anyHeld = anyHeld || heldCount_[Idx(m)] != 0;
    if (!anyHeld) chordLatched_ = false;
  }
}

std::vector<HostInputAction> HostInputMap::Feed(const InputSource &source,
                                                bool pressed) {
  std::vector<HostInputAction> out;
  if (source.kind == InputSource::Kind::Axis) return out; // see FeedAxis
  const auto it = bindings_.find(source);
  // An unbound source can still be mid-press if it was unbound while held.
  if (it == bindings_.end()) {
    if (!pressed) Apply(source, TrimuiControl::Count, false, out);
    return out;
  }
  Apply(source, it->second, pressed, out);
  return out;
}

std::vector<HostInputAction> HostInputMap::FeedAxis(int axis, float value) {
  std::vector<HostInputAction> out;
  if (!leftStickAsDpad_ || (axis != kLeftStickX && axis != kLeftStickY))
    return out;
  const int current = stickState_[axis];
  int next = 0;
  if (current != 0 && value * static_cast<float>(current) >= kStickRelease)
    next = current; // inside the hysteresis band: keep holding
  else if (value >= kStickPress)
    next = 1;
  else if (value <= -kStickPress)
    next = -1;
  if (next == current) return out;
  if (current != 0)
    Apply(InputSource::Axis(axis, current), StickControl(axis, current), false,
          out);
  if (next != 0)
    Apply(InputSource::Axis(axis, next), StickControl(axis, next), true, out);
  stickState_[axis] = next;
  return out;
}

std::vector<HostInputAction> HostInputMap::Tick(uint32_t nowMs) {
  std::vector<HostInputAction> out;
  const bool selectStart = heldCount_[Idx(TrimuiControl::Select)] != 0 &&
                           heldCount_[Idx(TrimuiControl::Start)] != 0;
  if (!selectStart) {
    comboTiming_ = false;
    if (heldCount_[Idx(TrimuiControl::Select)] == 0 &&
        heldCount_[Idx(TrimuiControl::Start)] == 0)
      comboLatched_ = false;
    return out;
  }
  if (comboLatched_ || chordLatched_) return out;
  if (!comboTiming_) {
    comboTiming_ = true;
    comboStartMs_ = nowMs;
    return out;
  }
  if (nowMs - comboStartMs_ < kComboHoldMs) return out;
  comboLatched_ = true;
  comboTiming_ = false;
  for (const auto m : {TrimuiControl::Select, TrimuiControl::Start}) {
    if (!delivered_[Idx(m)]) continue;
    delivered_[Idx(m)] = false;
    out.push_back({HostInputAction::Type::PadRelease, PadButtonFor(m)});
  }
  out.push_back({HostInputAction::Type::QuitRequest, EPBT_INVALID});
  return out;
}

std::vector<HostInputAction> HostInputMap::ReleaseAll() {
  std::vector<HostInputAction> out;
  for (int c = 0; c < kControls; ++c) {
    if (!delivered_[c]) continue;
    out.push_back({HostInputAction::Type::PadRelease,
                   PadButtonFor(static_cast<TrimuiControl>(c))});
  }
  held_.clear();
  heldCount_.fill(0);
  delivered_.fill(false);
  chordLatched_ = false;
  comboLatched_ = false;
  comboTiming_ = false;
  stickState_[0] = stickState_[1] = 0;
  return out;
}
