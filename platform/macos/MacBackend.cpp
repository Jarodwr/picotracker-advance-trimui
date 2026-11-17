// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "MacBackend.h"

namespace {

constexpr const char *kVirtualOutName = "picoTracker Out";
constexpr const char *kVirtualInName = "picoTracker In";

// Hardware endpoints must never resolve to our own virtual ports.
CoreMidiOutOptions hardwareOutOptions() {
  CoreMidiOutOptions options;
  options.excludeNames = {kVirtualInName};
  return options;
}

CoreMidiOutOptions virtualOutOptions() {
  CoreMidiOutOptions options;
  options.virtualName = kVirtualOutName;
  return options;
}

CoreMidiInOptions virtualInOptions() {
  CoreMidiInOptions options;
  options.virtualName = kVirtualInName;
  options.excludeNames = {kVirtualOutName};
  return options;
}

} // namespace

MacBackend::MacBackend(AudioSettings &audioSettings)
    : hardwareOut_("CoreMIDI Out", hardwareOutOptions()),
      virtualOut_("Virtual Out", virtualOutOptions()),
      in_("CoreMIDI In", virtualInOptions()), audio_(audioSettings),
      midi_(&in_, &hardwareOut_, &virtualOut_) {
  // Upstream starts input devices but only initialises outputs, so bring the
  // CoreMIDI client and virtual destination up here. A failure leaves the
  // input unavailable but the application still boots.
  inReady_ = in_.Init();
  // Outputs are initialised again by MidiService::Init (idempotent); doing it
  // now makes the endpoints visible to Status() and other apps before the
  // first playback starts them.
  hardwareReady_ = hardwareOut_.Init();
  virtualReady_ = virtualOut_.Init();
  midi_.SetPollHook([this] {
    hardwareOut_.Poll();
    virtualOut_.Poll();
  });
}

MacBackend::~MacBackend() = default;

void MacBackend::Install() {
  Audio::Install(&audio_);
  MidiService::Install(&midi_);
}

void MacBackend::Pump() {
  audio_.Pump();
  midi_.Poll();
}

PlatformStatus MacBackend::Status() const {
  PlatformStatus status;
  const HostAudioStatus audio = audio_.Status();
  status.audioAvailable = audio.playbackAvailable;
  status.audioApi = audio.api;
  status.audioDevice = audio.device;
  status.audioError = audio.error;
  if (in_.IsConnected()) {
    status.midiInAvailable = true;
    status.midiInName = in_.ConnectedSourceName();
  } else if (inReady_) {
    // No hardware source, but the virtual destination accepts input.
    status.midiInAvailable = true;
    status.midiInName = "picoTracker In (virtual)";
  }
  const std::string hardware =
      hardwareReady_ ? hardwareOut_.ConnectedDestinationName() : "";
  if (!hardware.empty()) {
    status.midiOutAvailable = true;
    status.midiOutName = hardware;
  } else if (virtualReady_) {
    status.midiOutAvailable = true;
    status.midiOutName = virtualOut_.ConnectedDestinationName() + " (virtual)";
  }
  return status;
}
