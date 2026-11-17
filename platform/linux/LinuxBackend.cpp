// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "LinuxBackend.h"
#include "HostRecord.h"

#include <utility>

namespace {
// Stands in for the device with --no-capture so HostRecord does not fall back
// to opening the SDL capture device itself.
class DisabledCaptureSource : public HostCaptureSource {
public:
  bool Open(HostCaptureSink *) override { return false; }
  void Close() override {}
};
} // namespace

LinuxBackend::LinuxBackend(AudioSettings &audioSettings,
                           LinuxBackendOptions options)
    : options_(std::move(options)), audio_(audioSettings), midi_(&midiIn_, &midiOut0_, &midiOut1_) {}

LinuxBackend::~LinuxBackend() = default;

void LinuxBackend::Install() {
  Audio::Install(&audio_);
  MidiService::Install(&midi_);
  if (!options_.captureEnabled) {
    recordingError_ = "capture disabled";
    HostRecordSetCapture(std::make_unique<HostAudioCapture>(
        std::make_unique<DisabledCaptureSource>()));
    return;
  }
  auto source = options_.captureSource ? options_.captureSource()
                                       : MakeSDLCaptureSource();
  auto capture = std::make_unique<HostAudioCapture>(std::move(source));
  recordingAvailable_ = capture->Open();
  if (recordingAvailable_) capture->Close();
  else recordingError_ = "capture device did not open";
  HostRecordSetCapture(std::move(capture));
}

void LinuxBackend::Pump() {
  audio_.Pump();
  midi_.Poll();
}

PlatformStatus LinuxBackend::Status() const {
  PlatformStatus status;
  const HostAudioStatus audio = audio_.Status();
  status.audioAvailable = audio.playbackAvailable;
  status.audioApi = audio.api;
  status.audioDevice = audio.device;
  status.audioError = audio.error;
  status.recordingAvailable = recordingAvailable_;
  status.recordingError = recordingError_;
  // MIDI is unsupported on Linux: both directions stay unavailable.
  return status;
}
