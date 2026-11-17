/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "SDLAudioDriver.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioOutDriver.h"

#include <memory>
#include <string>

class HostSilentOut final : public AudioOut {
public:
  bool Init() override { return true; }
  void Close() override {}
  bool Start() override { return true; }
  void Stop() override {}
  void Trigger() override {}
  int GetPlayedBufferPercentage() override { return 0; }
  etl::string<STRING_AUDIO_API_MAX> GetAudioAPI() override { return "silent"; }
  etl::string<STRING_AUDIO_DEVICE_MAX> GetAudioDevice() override {
    return "No audio device";
  }
  int GetAudioBufferSize() override { return 1024; }
  int GetAudioRequestedBufferSize() override { return 1024; }
  int GetAudioPreBufferCount() override { return 0; }
  double GetStreamTime() override { return 0.0; }
};

// Upstream AudioOutDriver whose identity reports the opened SDL device rather
// than the (empty) settings the driver was constructed with.
class HostSDLAudioOut final : public AudioOutDriver {
public:
  HostSDLAudioOut(SDLAudioDriver &driver)
      : AudioOutDriver(driver), sdl_(driver) {}
  etl::string<STRING_AUDIO_API_MAX> GetAudioAPI() override;
  etl::string<STRING_AUDIO_DEVICE_MAX> GetAudioDevice() override;
  int GetAudioBufferSize() override;

private:
  SDLAudioDriver &sdl_;
};

enum class HostAudioMode {
  Silent, // no device requested or the device could not be opened
  Sdl,    // real SDL output
};

// Audio availability for diagnostics and settings.
struct HostAudioStatus {
  HostAudioMode mode = HostAudioMode::Silent;
  bool playbackAvailable = false; // true only for a working SDL device
  std::string api;
  std::string device;
  std::string error; // why SDL was not used, empty if none
};

class HostAudio final : public Audio {
public:
  // useSdl=false installs the silent output only (tests, unsupported hosts).
  // If SDL cannot open a device the silent output is used so the application
  // still boots; HostAudioStatus reports that it is not real playback.
  HostAudio(AudioSettings &settings, bool useSdl = true)
      : Audio(settings), useSdl_(useSdl) {}
  ~HostAudio() override;
  void Init() override;
  void Close() override;

  // Firmware-thread pump: queues audio ahead of playback. No-op when silent.
  void Pump();
  HostAudioStatus Status() const { return status_; }

private:
  bool useSdl_;
  HostSilentOut silent_;
  std::unique_ptr<HostSDLAudioOut> sdlOut_;
  SDLAudioDriver *sdlDriver_ = nullptr;
  HostAudioStatus status_;
  bool initialized_ = false;
};
