// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostAudio.h"

#include <cstdio>

etl::string<STRING_AUDIO_API_MAX> HostSDLAudioOut::GetAudioAPI() {
  const char *api = SDL_GetCurrentAudioDriver();
  return api ? api : "SDL";
}

etl::string<STRING_AUDIO_DEVICE_MAX> HostSDLAudioOut::GetAudioDevice() {
  const auto status = sdl_.GetStatus();
  if (!status.deviceOpen) return "No audio device";
  return status.deviceName.c_str();
}

int HostSDLAudioOut::GetAudioBufferSize() {
  const auto status = sdl_.GetStatus();
  return status.bufferFrames > 0 ? status.bufferFrames
                                 : AudioOutDriver::GetAudioBufferSize();
}

HostAudio::~HostAudio() {
  // The output is owned here but registered with Audio; drop the pointer
  // before the owned object goes away.
  outputs_.clear();
}

void HostAudio::Init() {
  if (initialized_) return;
  initialized_ = true;

  if (useSdl_) {
    SDLAudioDriver *driver = new SDLAudioDriver(settings_);
    auto out = std::make_unique<HostSDLAudioOut>(*driver); // owns driver
    if (out->Init()) {
      sdlDriver_ = driver;
      sdlOut_ = std::move(out);
      const auto sdl = driver->GetStatus();
      status_.mode = HostAudioMode::Sdl;
      status_.playbackAvailable = true;
      status_.api = sdlOut_->GetAudioAPI().c_str();
      status_.device = sdl.deviceName;
      AddOutput(*sdlOut_);
    } else {
      status_.error = driver->GetStatus().error;
      std::fprintf(stderr, "SDL audio unavailable (%s); running silent\n",
                   status_.error.c_str());
    }
  }

  if (!sdlOut_) {
    silent_.Init();
    status_.api = silent_.GetAudioAPI().c_str();
    status_.device = silent_.GetAudioDevice().c_str();
    AddOutput(silent_);
  }
  settings_.audioAPI_ = status_.api.c_str();
  settings_.audioDevice_ = status_.device.c_str();
}

void HostAudio::Close() {
  if (sdlOut_) sdlOut_->Close();
  else silent_.Close();
}

void HostAudio::Pump() {
  if (sdlDriver_) sdlDriver_->Pump();
}
