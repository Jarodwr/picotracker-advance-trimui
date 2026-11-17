// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostAudioCapture.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

HostAudioCapture::HostAudioCapture(std::unique_ptr<HostCaptureSource> source,
                                   size_t ringFrames)
    : source_(std::move(source)), capacityFrames_(ringFrames) {}

HostAudioCapture::~HostAudioCapture() { Close(); }

bool HostAudioCapture::Open() {
  if (state_ == State::Open)
    return true;
  if (!source_ || capacityFrames_ == 0) {
    state_ = State::Unavailable;
    return false;
  }
  ring_.assign(capacityFrames_ * kCaptureChannels, 0);
  head_ = 0;
  tail_ = 0;
  overrunBlocks_ = 0;
  overrunFrames_ = 0;
  level_ = 0;
  if (!source_->Open(this)) {
    state_ = State::Unavailable;
    return false;
  }
  state_ = State::Open;
  return true;
}

void HostAudioCapture::Close() {
  if (source_ && state_ == State::Open)
    source_->Close();
  if (state_ == State::Open)
    state_ = State::Closed;
  level_ = 0;
}

size_t HostAudioCapture::QueuedFrames() const {
  return static_cast<size_t>(head_.load(std::memory_order_acquire) -
                             tail_.load(std::memory_order_acquire));
}

void HostAudioCapture::OnCaptureBlock(const int16_t *in, size_t frames) {
  if (frames == 0 || ring_.empty())
    return;
  int peak = 0;
  for (size_t i = 0; i < frames * kCaptureChannels; ++i)
    peak = std::max(peak, std::abs(static_cast<int>(in[i])));
  level_.store(peak, std::memory_order_relaxed);

  uint64_t head = head_.load(std::memory_order_relaxed);
  uint64_t tail = tail_.load(std::memory_order_acquire);
  if (capacityFrames_ - static_cast<size_t>(head - tail) < frames) {
    // Whole-block drop keeps the consumer the only writer of tail_.
    overrunBlocks_.fetch_add(1, std::memory_order_relaxed);
    overrunFrames_.fetch_add(frames, std::memory_order_relaxed);
    return;
  }
  size_t pos = static_cast<size_t>(head % capacityFrames_);
  size_t first = std::min(frames, capacityFrames_ - pos);
  std::memcpy(&ring_[pos * kCaptureChannels], in,
              first * kCaptureChannels * sizeof(int16_t));
  if (first < frames)
    std::memcpy(&ring_[0], in + first * kCaptureChannels,
                (frames - first) * kCaptureChannels * sizeof(int16_t));
  head_.store(head + frames, std::memory_order_release);
}

size_t HostAudioCapture::Read(int16_t *out, size_t maxFrames) {
  if (state_ != State::Open || ring_.empty())
    return 0;
  uint64_t tail = tail_.load(std::memory_order_relaxed);
  uint64_t head = head_.load(std::memory_order_acquire);
  size_t n = std::min(maxFrames, static_cast<size_t>(head - tail));
  if (n == 0)
    return 0;
  size_t pos = static_cast<size_t>(tail % capacityFrames_);
  size_t first = std::min(n, capacityFrames_ - pos);
  std::memcpy(out, &ring_[pos * kCaptureChannels],
              first * kCaptureChannels * sizeof(int16_t));
  if (first < n)
    std::memcpy(out + first * kCaptureChannels, &ring_[0],
                (n - first) * kCaptureChannels * sizeof(int16_t));
  tail_.store(tail + n, std::memory_order_release);
  return n;
}

namespace {

class SDLCaptureSource : public HostCaptureSource {
public:
  ~SDLCaptureSource() override { Close(); }

  bool Open(HostCaptureSink *sink) override {
    if (device_ != 0)
      return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      std::fprintf(stderr, "capture: SDL audio init failed: %s\n",
                   SDL_GetError());
      return false;
    }
    sink_ = sink;
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = kCaptureSampleRate;
    want.format = AUDIO_S16SYS;
    want.channels = kCaptureChannels;
    want.samples = 1024;
    want.callback = &SDLCaptureSource::Callback;
    want.userdata = this;
    SDL_AudioSpec have;
    SDL_zero(have);
    device_ = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    if (device_ == 0) {
      std::fprintf(stderr, "capture: no capture device: %s\n", SDL_GetError());
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      return false;
    }
    SDL_PauseAudioDevice(device_, 0);
    return true;
  }

  void Close() override {
    if (device_ == 0)
      return;
    SDL_CloseAudioDevice(device_); // joins the callback thread
    device_ = 0;
    sink_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }

private:
  static void Callback(void *self, Uint8 *stream, int len) {
    auto *s = static_cast<SDLCaptureSource *>(self);
    if (s->sink_)
      s->sink_->OnCaptureBlock(
          reinterpret_cast<const int16_t *>(stream),
          static_cast<size_t>(len) / (kCaptureChannels * sizeof(int16_t)));
  }

  SDL_AudioDeviceID device_ = 0;
  HostCaptureSink *sink_ = nullptr;
};

} // namespace

std::unique_ptr<HostCaptureSource> MakeSDLCaptureSource() {
  return std::make_unique<SDLCaptureSource>();
}
