/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Services/Audio/AudioDriver.h"
#include "Services/Audio/AudioOutDriver.h"

#include <SDL.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

// Snapshot of what the SDL audio device actually is right now.
struct SDLAudioStatus {
  bool deviceOpen = false;     // SDL device is open and usable
  bool silentFallback = false; // device lost; time keeps moving, no sound
  bool deviceLost = false;     // a lost device was detected since Init
  int sampleRate = 0;          // obtained rate (SDL converts to 44100)
  int channels = 0;
  int bufferFrames = 0; // obtained device buffer size, in frames
  std::string deviceName;
  std::string error; // last open/queue failure, empty if none
};

// SDL AudioDriver for the upstream AudioOutDriver. Output is 44.1 kHz,
// signed 16-bit, interleaved stereo; SDL converts if the hardware differs.
//
// Threading: every AudioDriver call and Pump() belong to the firmware thread.
// No SDL audio callback is installed (SDL_QueueAudio is used), so SDL's audio
// thread never touches firmware objects. Status/time getters may be called
// from any thread.
class SDLAudioDriver final : public AudioDriver {
public:
  static constexpr int kSampleRate = 44100;
  static constexpr int kChannels = 2;

  explicit SDLAudioDriver(AudioSettings &settings);
  ~SDLAudioDriver() override;

  bool InitDriver() override;
  void CloseDriver() override;
  bool StartDriver() override;
  void StopDriver() override;

  bool Interlaced() override { return true; }
  int GetPlayedBufferPercentage() override;
  double GetStreamTime() override;

  // Host event-loop pump (firmware thread). While playing, tops the queue up
  // to the target depth: asks upstream for a buffer, queues it, then raises
  // the driver tick. Returns the number of chunks queued.
  int Pump();

  SDLAudioStatus GetStatus() const;

private:
  enum class Mode { Closed, Device, Silent };

  int drainPool();
  int64_t queuedFrames() const;       // caller holds mutex_
  int64_t silentConsumedFrames() const; // caller holds mutex_
  void enterSilentFallback(const char *reason); // caller holds mutex_
  bool deviceStillAlive() const;

  mutable std::mutex mutex_;
  Mode mode_ = Mode::Closed;
  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec obtained_{};
  SDLAudioStatus status_;
  bool initializedSubsystem_ = false;

  int64_t submittedFrames_ = 0;
  int lastChunkFrames_ = 0;
  int targetFrames_ = 0;

  // Silent fallback clock.
  int64_t silentBaseFrames_ = 0;
  std::chrono::steady_clock::time_point silentStart_;
};

// Builds an AudioOutDriver that owns a new SDLAudioDriver. The SDLAudioDriver
// pointer is for the host pump/status and stays valid for the output's life.
std::unique_ptr<AudioOutDriver> MakeSDLAudioOut(AudioSettings &settings,
                                                SDLAudioDriver *&driverOut);
