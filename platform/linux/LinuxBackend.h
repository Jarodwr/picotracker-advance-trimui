/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "HostAudio.h"
#include "HostAudioCapture.h"
#include "HostMidi.h"

#include <functional>
#include <memory>
#include <string>

// Same shape as MacBackend's PlatformStatus; the two backends are never built
// together. Move it to platform/desktop once a third backend needs it.
struct PlatformStatus {
  bool audioAvailable = false;
  std::string audioApi;
  std::string audioDevice;
  std::string audioError; // why real audio is unavailable, empty if none
  bool midiInAvailable = false;
  std::string midiInName;
  bool midiOutAvailable = false;
  std::string midiOutName;
  bool recordingAvailable = false; // capture really opened at startup
  std::string recordingError;      // why not, empty if available or disabled
};

struct LinuxBackendOptions {
  bool captureEnabled = true; // false for --no-capture: recording stays off
  // Capture source factory; null uses the SDL capture device. Tests inject a
  // source that fails to open.
  std::function<std::unique_ptr<HostCaptureSource>()> captureSource;
};

// Linux backend: SDL audio plus explicitly unsupported MIDI (no ALSA/JACK
// implementation yet). The same executable boots when no audio device exists.
class LinuxBackend {
public:
  explicit LinuxBackend(AudioSettings &audioSettings,
                        LinuxBackendOptions options = {});
  ~LinuxBackend();
  LinuxBackend(const LinuxBackend &) = delete;
  LinuxBackend &operator=(const LinuxBackend &) = delete;

  // Registers Audio and MidiService with the firmware's factories and probes
  // capture: the device is opened once to learn whether recording works, then
  // closed again and handed to HostRecord, which reopens it on demand. Absent
  // or failing capture never stops the boot.
  void Install();

  // Firmware-thread pump, called each host event-loop iteration.
  void Pump();

  PlatformStatus Status() const;

private:
  LinuxBackendOptions options_;
  bool recordingAvailable_ = false;
  std::string recordingError_;
  HostAudio audio_;
  // Explicit unsupported MIDI: every slot is a null device. They are owned
  // here, declared before midi_, because upstream's ~MidiService() stops the
  // output devices during destruction. HostMidiService's own internal null
  // devices are already destroyed by then, which aborts with "pure virtual
  // method called".
  HostNullMidiIn midiIn_;
  HostNullMidiOut midiOut0_;
  HostNullMidiOut midiOut1_;
  HostMidiService midi_;
};
