/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ADR 0018: wrapper-owned audio capture for the firmware's recording. A
// HostCaptureSource (SDL capture device, or a fake in tests) pushes fixed
// format blocks into a preallocated single-producer single-consumer ring; the
// consumer drains it with Read(). Nothing here allocates after Open(), and a
// source that fails to open leaves the capture "unavailable" instead of
// throwing or crashing.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

constexpr int kCaptureSampleRate = 44100;
constexpr int kCaptureChannels = 2; // interleaved signed 16-bit

class HostCaptureSink {
public:
  virtual ~HostCaptureSink() = default;
  // Called on the source's audio thread. Must not allocate or block.
  virtual void OnCaptureBlock(const int16_t *interleaved, size_t frames) = 0;
};

class HostCaptureSource {
public:
  virtual ~HostCaptureSource() = default;
  // Start delivering 44.1 kHz stereo S16 blocks to the sink. False when the
  // device cannot be opened in exactly that format.
  virtual bool Open(HostCaptureSink *sink) = 0;
  // Stop delivery; after it returns the sink is no longer called.
  virtual void Close() = 0;
};

// SDL capture device (ALSA on the Trimui, CoreAudio on macOS, the dummy driver
// under SDL_AUDIODRIVER=dummy, which delivers silence).
std::unique_ptr<HostCaptureSource> MakeSDLCaptureSource();

class HostAudioCapture : public HostCaptureSink {
public:
  enum class State { Closed, Open, Unavailable };

  // The ring holds ringFrames frames (sizing guidance: ADR 0018).
  explicit HostAudioCapture(std::unique_ptr<HostCaptureSource> source,
                            size_t ringFrames = kCaptureSampleRate);
  ~HostAudioCapture() override;
  HostAudioCapture(const HostAudioCapture &) = delete;
  HostAudioCapture &operator=(const HostAudioCapture &) = delete;

  // True when capturing. False leaves State() == Unavailable; may be retried.
  bool Open();
  void Close();
  State GetState() const { return state_; }
  bool Available() const { return state_ == State::Open; }

  // Copy up to maxFrames queued frames out (consumer thread). Returns frames
  // copied; 0 when nothing is queued or the capture is not open.
  size_t Read(int16_t *out, size_t maxFrames);
  size_t QueuedFrames() const;

  // Blocks dropped because the ring was full, and the frames in them.
  uint64_t OverrunBlocks() const { return overrunBlocks_; }
  uint64_t OverrunFrames() const { return overrunFrames_; }
  // Peak absolute sample (0..32767) of the latest block; 0 when not open.
  int Level() const { return level_; }

  void OnCaptureBlock(const int16_t *interleaved, size_t frames) override;

private:
  std::unique_ptr<HostCaptureSource> source_;
  std::vector<int16_t> ring_;
  size_t capacityFrames_;
  // Monotonic frame counters; head_ is only written by the producer, tail_ by
  // the consumer.
  std::atomic<uint64_t> head_{0};
  std::atomic<uint64_t> tail_{0};
  std::atomic<uint64_t> overrunBlocks_{0};
  std::atomic<uint64_t> overrunFrames_{0};
  std::atomic<int> level_{0};
  State state_ = State::Closed;
};
