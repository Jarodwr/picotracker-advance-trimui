/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "CoreMidiIn.h"
#include "CoreMidiOut.h"
#include "HostAudio.h"
#include "HostMidi.h"

#include <string>

// What the host can really do right now, for diagnostics and UI settings.
// Silent audio and null MIDI slots never count as available.
struct PlatformStatus {
  bool audioAvailable = false;
  std::string audioApi;
  std::string audioDevice;
  std::string audioError; // why real audio is unavailable, empty if none
  bool midiInAvailable = false;
  std::string midiInName;
  bool midiOutAvailable = false;
  std::string midiOutName;
  // Recording needs host capture support, which is not implemented yet.
  bool recordingAvailable = false;
};

// macOS backend: composes SDL audio with CoreMIDI devices and installs them
// as the firmware's Audio and MidiService (ADR 0013). Construct after the
// firmware's System/FileSystem/Timer services are installed and before the
// application initialises. The same executable boots when devices are absent.
//
// MIDI layout (upstream allows two slots per direction):
//   output 0: CoreMIDI hardware destination, "CoreMIDI Out"
//   output 1: virtual CoreMIDI source "picoTracker Out", "Virtual Out"
//   input  0: CoreMIDI hardware source plus virtual destination
//             "picoTracker In"; reported to upstream as the Usb source
class MacBackend {
public:
  explicit MacBackend(AudioSettings &audioSettings);
  ~MacBackend();
  MacBackend(const MacBackend &) = delete;
  MacBackend &operator=(const MacBackend &) = delete;

  // Registers Audio and MidiService with the firmware's factories.
  void Install();

  // Firmware-thread pump, called each host event-loop iteration.
  void Pump();

  PlatformStatus Status() const;

private:
  CoreMidiOut hardwareOut_;
  CoreMidiOut virtualOut_;
  CoreMidiIn in_;
  bool inReady_ = false;
  bool hardwareReady_ = false;
  bool virtualReady_ = false;
  HostAudio audio_;
  HostMidiService midi_;
};
