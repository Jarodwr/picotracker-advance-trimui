// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SDLJoystickSource.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

// Compiled source table (ADR 0016 section 1), the inverse of the default
// bindings in HostInputMap. Indices follow the device's SDL enumeration
// ('Xbox 360 Controller': 11 buttons, 6 axes, 1 hat); face-button positions
// still need a person pressing them (ADR 0016).
bool DefaultSource(TrimuiControl control, InputSource &out) {
  using D = InputSource::Direction;
  switch (control) {
  case TrimuiControl::DpadUp: out = InputSource::Hat(0, D::HatUp); return true;
  case TrimuiControl::DpadDown: out = InputSource::Hat(0, D::HatDown); return true;
  case TrimuiControl::DpadLeft: out = InputSource::Hat(0, D::HatLeft); return true;
  case TrimuiControl::DpadRight: out = InputSource::Hat(0, D::HatRight); return true;
  case TrimuiControl::South: out = InputSource::Button(0); return true;
  case TrimuiControl::East: out = InputSource::Button(1); return true;
  case TrimuiControl::West: out = InputSource::Button(2); return true;
  case TrimuiControl::North: out = InputSource::Button(3); return true;
  case TrimuiControl::L1: out = InputSource::Button(4); return true;
  case TrimuiControl::R1: out = InputSource::Button(5); return true;
  case TrimuiControl::Select: out = InputSource::Button(6); return true;
  case TrimuiControl::Start: out = InputSource::Button(7); return true;
  case TrimuiControl::Menu: out = InputSource::Button(8); return true;
  case TrimuiControl::L3: out = InputSource::Button(9); return true;
  case TrimuiControl::R3: out = InputSource::Button(10); return true;
  default: return false; // L2/R2 are analog axes 2 and 5: no button source
  }
}

// Control whose pad button is `button` (the one HostInputMap resolves).
TrimuiControl NativeControl(GUIEventPadButtonType button) {
  for (int i = 0; i < static_cast<int>(TrimuiControl::Count); ++i) {
    const auto control = static_cast<TrimuiControl>(i);
    if (HostInputMap::PadButtonFor(control) == button) return control;
  }
  return TrimuiControl::Count;
}

} // namespace

SDLJoystickSource::SDLJoystickSource() = default;

SDLJoystickSource::~SDLJoystickSource() {
  for (auto &d : devices_)
    if (d.second) SDL_JoystickClose(d.second);
}

void SDLJoystickSource::ApplyProfile(const HostInputProfile &profile) {
  // Start from an empty table, then bind each Advance button's source (the one
  // the profile's Trimui control lives on) to the control HostInputMap turns
  // into that pad button. Inert controls keep their default sources.
  for (int i = 0; i < static_cast<int>(TrimuiControl::Count); ++i) {
    InputSource source;
    if (DefaultSource(static_cast<TrimuiControl>(i), source)) map_.Unbind(source);
  }
  bool claimed[static_cast<int>(TrimuiControl::Count)] = {};
  for (const auto &entry : profile.entries) {
    const TrimuiControl chosen = profile.ControlFor(entry.button);
    const TrimuiControl native = NativeControl(entry.button);
    InputSource source;
    if (chosen == TrimuiControl::Count || native == TrimuiControl::Count ||
        !DefaultSource(chosen, source))
      continue;
    claimed[static_cast<int>(chosen)] = true;
    map_.Bind(source, native);
  }
  for (int i = 0; i < static_cast<int>(TrimuiControl::Count); ++i) {
    const auto control = static_cast<TrimuiControl>(i);
    InputSource source;
    if (!claimed[i] && HostInputMap::PadButtonFor(control) == EPBT_INVALID &&
        DefaultSource(control, source))
      map_.Bind(source, control);
  }
  map_.SetLeftStickAsDpad(profile.leftStickAsDpad);
}

bool SDLJoystickSource::LoadProfileFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    ApplyProfile(HostInputProfile::Defaults());
    if (logInput_)
      std::fprintf(stderr, "input: no profile at %s, using defaults\n", path.c_str());
    return false;
  }
  std::stringstream text;
  text << file.rdbuf();
  const HostInputParseResult result = ParseHostInputProfile(text.str());
  for (const auto &line : result.rejected)
    std::fprintf(stderr, "input: %s:%d rejected (%s): %s\n", path.c_str(),
                 line.line, line.reason.c_str(), line.text.c_str());
  ApplyProfile(result.profile);
  return true;
}

bool SDLJoystickSource::Handles(Uint32 type) {
  switch (type) {
  case SDL_JOYBUTTONDOWN:
  case SDL_JOYBUTTONUP:
  case SDL_JOYHATMOTION:
  case SDL_JOYAXISMOTION:
  case SDL_JOYDEVICEADDED:
  case SDL_JOYDEVICEREMOVED:
    return true;
  default:
    return false;
  }
}

void SDLJoystickSource::LogPress(const char *what, int index, int extra) {
  if (!logInput_) return;
  if (extra >= 0) std::fprintf(stderr, "input: %s %d %d\n", what, index, extra);
  else std::fprintf(stderr, "input: %s %d\n", what, index);
}

void SDLJoystickSource::DeviceAdded(int deviceIndex) {
  SDL_Joystick *joystick = SDL_JoystickOpen(deviceIndex);
  if (!joystick) {
    std::fprintf(stderr, "input: could not open joystick %d: %s\n", deviceIndex,
                 SDL_GetError());
    return;
  }
  const SDL_JoystickID id = SDL_JoystickInstanceID(joystick);
  if (devices_.count(id)) return; // same device reported twice
  devices_[id] = joystick;
  if (logInput_)
    std::fprintf(stderr, "input: joystick %d '%s' buttons=%d axes=%d hats=%d\n",
                 deviceIndex, SDL_JoystickName(joystick) ? SDL_JoystickName(joystick) : "",
                 SDL_JoystickNumButtons(joystick), SDL_JoystickNumAxes(joystick),
                 SDL_JoystickNumHats(joystick));
}

std::vector<HostInputAction> SDLJoystickSource::DeviceRemoved(SDL_JoystickID id) {
  auto found = devices_.find(id);
  if (found != devices_.end()) {
    if (found->second) SDL_JoystickClose(found->second);
    devices_.erase(found);
  }
  hatState_.clear();
  axisLogged_.clear();
  if (logInput_) std::fprintf(stderr, "input: joystick %d removed\n", static_cast<int>(id));
  // Whatever the lost device held must not stay pressed.
  return map_.ReleaseAll();
}

std::vector<HostInputAction> SDLJoystickSource::HatMotion(int hat, Uint8 value) {
  struct Dir { Uint8 mask; InputSource::Direction direction; };
  static const Dir dirs[] = {{SDL_HAT_UP, InputSource::HatUp},
                             {SDL_HAT_DOWN, InputSource::HatDown},
                             {SDL_HAT_LEFT, InputSource::HatLeft},
                             {SDL_HAT_RIGHT, InputSource::HatRight}};
  const Uint8 before = hatState_[hat];
  hatState_[hat] = value;
  std::vector<HostInputAction> out;
  for (const auto &d : dirs) {
    const bool was = (before & d.mask) != 0;
    const bool now = (value & d.mask) != 0;
    if (was == now) continue;
    if (now) LogPress("hat", hat, d.direction);
    const auto actions = map_.Feed(InputSource::Hat(hat, d.direction), now);
    out.insert(out.end(), actions.begin(), actions.end());
  }
  return out;
}

std::vector<HostInputAction> SDLJoystickSource::Handle(const SDL_Event &event) {
  switch (event.type) {
  case SDL_JOYDEVICEADDED:
    DeviceAdded(event.jdevice.which);
    return {};
  case SDL_JOYDEVICEREMOVED:
    return DeviceRemoved(event.jdevice.which);
  case SDL_JOYBUTTONDOWN:
  case SDL_JOYBUTTONUP: {
    const bool pressed = event.type == SDL_JOYBUTTONDOWN;
    if (pressed) LogPress("button", event.jbutton.button, -1);
    return map_.Feed(InputSource::Button(event.jbutton.button), pressed);
  }
  case SDL_JOYHATMOTION:
    return HatMotion(event.jhat.hat, event.jhat.value);
  case SDL_JOYAXISMOTION: {
    const float value = event.jaxis.value < 0 ? event.jaxis.value / 32768.0f
                                              : event.jaxis.value / 32767.0f;
    const int axis = event.jaxis.axis;
    const float mag = value < 0 ? -value : value;
    if (mag >= HostInputMap::kStickPress && !axisLogged_[axis]) {
      axisLogged_[axis] = true;
      LogPress("axis", axis, value < 0 ? -1 : 1);
    } else if (mag < HostInputMap::kStickRelease) axisLogged_[axis] = false;
    return map_.FeedAxis(axis, value);
  }
  default:
    return {};
  }
}
