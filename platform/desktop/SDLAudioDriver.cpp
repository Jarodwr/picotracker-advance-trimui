// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SDLAudioDriver.h"

#include <algorithm>

namespace {

constexpr int kBytesPerFrame = SDLAudioDriver::kChannels * sizeof(int16_t);
constexpr int kDefaultDeviceFrames = 1024;
constexpr int kMaxQueuedFrames = 8192;
constexpr int kMaxChunksPerPump = 4;

int powerOfTwoAtLeast(int v) {
  int p = 256;
  while (p < v && p < 8192)
    p <<= 1;
  return p;
}

} // namespace

SDLAudioDriver::SDLAudioDriver(AudioSettings &settings)
    : AudioDriver(settings) {}

SDLAudioDriver::~SDLAudioDriver() { CloseDriver(); }

bool SDLAudioDriver::InitDriver() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mode_ != Mode::Closed)
    return true;

  status_ = SDLAudioStatus{};
  submittedFrames_ = 0;
  lastChunkFrames_ = 0;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      status_.error = SDL_GetError();
      return false;
    }
    initializedSubsystem_ = true;
  }

  const int requested = settings_.bufferSize_ > 0 ? settings_.bufferSize_
                                                  : kDefaultDeviceFrames;
  SDL_AudioSpec want{};
  want.freq = kSampleRate;
  want.format = AUDIO_S16SYS;
  want.channels = kChannels;
  want.samples = static_cast<Uint16>(powerOfTwoAtLeast(requested));
  want.callback = nullptr; // queue mode: SDL's thread never calls firmware

  const char *name =
      settings_.audioDevice_.empty() ? nullptr : settings_.audioDevice_.c_str();
  // allowed_changes == 0: SDL presents our format and converts for hardware.
  device_ = SDL_OpenAudioDevice(name, 0, &want, &obtained_, 0);
  if (device_ == 0 && name != nullptr) {
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained_, 0);
  }
  if (device_ == 0) {
    status_.error = SDL_GetError();
    if (initializedSubsystem_) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      initializedSubsystem_ = false;
    }
    return false;
  }

  targetFrames_ =
      std::min(kMaxQueuedFrames, std::max(2 * static_cast<int>(obtained_.samples), 2048));
  mode_ = Mode::Device;
  status_.deviceOpen = true;
  status_.sampleRate = obtained_.freq;
  status_.channels = obtained_.channels;
  status_.bufferFrames = obtained_.samples;
  status_.deviceName = name ? name : "default";
  return true;
}

void SDLAudioDriver::CloseDriver() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  if (initializedSubsystem_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initializedSubsystem_ = false;
  }
  mode_ = Mode::Closed;
  status_.deviceOpen = false;
  status_.silentFallback = false;
}

bool SDLAudioDriver::StartDriver() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mode_ == Mode::Closed)
    return false;
  submittedFrames_ = 0;
  lastChunkFrames_ = 0;
  if (mode_ == Mode::Device) {
    SDL_ClearQueuedAudio(device_);
    SDL_PauseAudioDevice(device_, 0);
  } else {
    silentBaseFrames_ = 0;
    silentStart_ = std::chrono::steady_clock::now();
  }
  return true;
}

void SDLAudioDriver::StopDriver() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mode_ == Mode::Device) {
    SDL_PauseAudioDevice(device_, 1);
    SDL_ClearQueuedAudio(device_);
  }
  submittedFrames_ = 0;
  lastChunkFrames_ = 0;
}

bool SDLAudioDriver::deviceStillAlive() const {
  return SDL_GetAudioDeviceStatus(device_) != SDL_AUDIO_STOPPED;
}

void SDLAudioDriver::enterSilentFallback(const char *reason) {
  // Frames already played before the loss count as consumed.
  const int64_t consumed = std::max<int64_t>(0, submittedFrames_ - queuedFrames());
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  silentBaseFrames_ = consumed;
  silentStart_ = std::chrono::steady_clock::now();
  mode_ = Mode::Silent;
  status_.deviceOpen = false;
  status_.silentFallback = true;
  status_.deviceLost = true;
  status_.error = reason;
}

int64_t SDLAudioDriver::silentConsumedFrames() const {
  const auto elapsed = std::chrono::steady_clock::now() - silentStart_;
  const double secs = std::chrono::duration<double>(elapsed).count();
  const int64_t consumed = silentBaseFrames_ + static_cast<int64_t>(secs * kSampleRate);
  return std::min(consumed, submittedFrames_);
}

int64_t SDLAudioDriver::queuedFrames() const {
  switch (mode_) {
  case Mode::Device:
    return SDL_GetQueuedAudioSize(device_) / kBytesPerFrame;
  case Mode::Silent:
    return submittedFrames_ - silentConsumedFrames();
  case Mode::Closed:
    break;
  }
  return 0;
}

int SDLAudioDriver::GetPlayedBufferPercentage() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mode_ == Mode::Closed || lastChunkFrames_ <= 0)
    return 0;
  const int64_t queued = queuedFrames();
  if (queued >= lastChunkFrames_)
    return 0;
  return static_cast<int>(100 - queued * 100 / lastChunkFrames_);
}

double SDLAudioDriver::GetStreamTime() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mode_ == Mode::Closed)
    return 0.0;
  // Frames still queued exclude those already handed to hardware, so this
  // slightly overestimates progress by up to one device buffer.
  const int64_t played = std::max<int64_t>(0, submittedFrames_ - queuedFrames());
  return static_cast<double>(played) / kSampleRate;
}

// Moves one chunk, if upstream produced one, from the upstream pool into SDL.
int SDLAudioDriver::drainPool() {
  AudioBufferData &slot = pool_[poolPlayPosition_];
  if (slot.empty_)
    return 0;
  const int bytes = slot.size_;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mode_ == Mode::Device) {
      if (SDL_QueueAudio(device_, slot.buffer_, bytes) != 0) {
        enterSilentFallback(SDL_GetError());
      }
    }
    if (mode_ != Mode::Closed) {
      submittedFrames_ += bytes / kBytesPerFrame;
      lastChunkFrames_ = bytes / kBytesPerFrame;
    }
  }
  slot.empty_ = true;
  poolPlayPosition_ = (poolPlayPosition_ + 1) % SOUND_BUFFER_COUNT;
  return 1;
}

int SDLAudioDriver::Pump() {
  if (!isPlaying_)
    return 0;

  int chunks = 0;
  for (int i = 0; i < kMaxChunksPerPump; ++i) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (mode_ == Mode::Closed)
        break;
      if (mode_ == Mode::Device && !deviceStillAlive())
        enterSilentFallback("audio device lost");
      if (queuedFrames() >= targetFrames_)
        break;
    }
    // Upstream renders on this (firmware) thread and calls AddBuffer.
    OnNewBufferNeeded();
    if (drainPool() == 0)
      break;
    ++chunks;
    // Tick after the chunk is queued so MIDI output follows its audio.
    onAudioBufferTick();
  }
  return chunks;
}

SDLAudioStatus SDLAudioDriver::GetStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

std::unique_ptr<AudioOutDriver> MakeSDLAudioOut(AudioSettings &settings,
                                                SDLAudioDriver *&driverOut) {
  auto *driver = new SDLAudioDriver(settings);
  driverOut = driver;
  return std::make_unique<AudioOutDriver>(*driver); // takes ownership
}
