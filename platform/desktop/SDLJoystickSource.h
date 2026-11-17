/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ADR 0016 section 4: SDL joystick events -> HostInputMap sources. Only this
// adapter touches SDL joystick types; the mapping, exit chord and left-stick
// state machine live in HostInputMap, the profile in HostInputProfile.

#include "HostInputMap.h"
#include "HostInputProfile.h"

#include <SDL.h>

#include <map>
#include <string>
#include <vector>

class SDLJoystickSource {
public:
  SDLJoystickSource();
  ~SDLJoystickSource();
  SDLJoystickSource(const SDLJoystickSource &) = delete;
  SDLJoystickSource &operator=(const SDLJoystickSource &) = delete;

  // With logging on, each joystick name, its button/axis/hat counts and every
  // press index go to stderr so the profile can be completed on the device.
  void SetLogInput(bool enabled) { logInput_ = enabled; }

  // Rebuild the source table from a profile (Advance button -> Trimui control,
  // resolved to SDL indices through the compiled source table).
  void ApplyProfile(const HostInputProfile &profile);
  // Load input.ini; a missing or unreadable file keeps the compiled default,
  // rejected lines are logged to stderr. Returns true when a file was read.
  bool LoadProfileFile(const std::string &path);

  // True for the joystick event types this source consumes.
  static bool Handles(Uint32 type);
  // Translate one SDL event into pad actions (empty when nothing changed).
  std::vector<HostInputAction> Handle(const SDL_Event &event);

  // Select+Start hold detection needs a clock (HostInputMap::Tick).
  std::vector<HostInputAction> Tick(Uint32 nowMs) { return map_.Tick(nowMs); }

  int DeviceCount() const { return static_cast<int>(devices_.size()); }
  const HostInputMap &Map() const { return map_; }

private:
  void DeviceAdded(int deviceIndex);
  std::vector<HostInputAction> DeviceRemoved(SDL_JoystickID id);
  std::vector<HostInputAction> HatMotion(int hat, Uint8 value);
  void LogPress(const char *what, int index, int extra);

  HostInputMap map_;
  std::map<SDL_JoystickID, SDL_Joystick *> devices_;
  std::map<int, Uint8> hatState_;
  std::map<int, bool> axisLogged_;
  bool logInput_ = false;
};
