/*
 * Copyright (c) 2018 Discodirt
 * Copyright (c) 2024 xiphonics, inc.
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: BSD-3-Clause AND GPL-3.0-only
 *
 * BSD-3-Clause applies to the declarations mirrored from the upstream picoTracker
 * firmware; GPL-3.0-only applies to the wrapper's own additions. Both apply to
 * the file as a whole.
 */

#pragma once

// Recording stand-in for the upstream Player, so the real MidiInDevice.cpp
// parser and router can be linked without the Song/Player graph. It records
// every call the router makes.
#include "Services/Midi/MidiInputSource.h"

#include <cstdint>
#include <string>
#include <vector>

class Player {
public:
  static Player *GetInstance() {
    static Player instance;
    return &instance;
  }
  static Player *GetExistingInstance() { return GetInstance(); }

  void PlayMidiNote(MidiInputSource source, uint8_t channel, uint8_t instrument,
                    uint8_t note, uint8_t velocity);
  void StopMidiNote(MidiInputSource source, uint8_t channel, uint8_t note);
  void ReleaseMidiInputSource(MidiInputSource source);
  void OnSongStartButton(unsigned int from, unsigned int to, bool requestStop,
                         bool forceImmediate);
  void Stop();
  bool IsRunning() { return running; }

  bool running = false;
  std::vector<std::string> calls;
};
