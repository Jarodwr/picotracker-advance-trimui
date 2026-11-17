// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostAudioCapture.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace {

class FakeSource : public HostCaptureSource {
public:
  explicit FakeSource(bool opens) : opens_(opens) {}
  bool Open(HostCaptureSink *sink) override {
    if (!opens_)
      return false;
    sink_ = sink;
    open_ = true;
    return true;
  }
  void Close() override { open_ = false; }
  // Push `frames` frames whose left sample counts up from `first`, right = -left.
  void Push(int first, size_t frames) {
    std::vector<int16_t> b(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
      b[2 * i] = static_cast<int16_t>(first + static_cast<int>(i));
      b[2 * i + 1] = static_cast<int16_t>(-(first + static_cast<int>(i)));
    }
    sink_->OnCaptureBlock(b.data(), frames);
  }
  bool open_ = false;

private:
  bool opens_;
  HostCaptureSink *sink_ = nullptr;
};

HostAudioCapture Make(FakeSource *&raw, bool opens, size_t ring) {
  auto src = std::make_unique<FakeSource>(opens);
  raw = src.get();
  return HostAudioCapture(std::move(src), ring);
}

void RingWrap() {
  FakeSource *src;
  HostAudioCapture cap = Make(src, true, 8);
  CHECK(cap.GetState() == HostAudioCapture::State::Closed);
  CHECK(cap.Open() && cap.Available());
  int16_t out[32];
  src->Push(1, 6);
  CHECK(cap.QueuedFrames() == 6);
  CHECK(cap.Read(out, 4) == 4);
  CHECK(out[0] == 1 && out[1] == -1 && out[6] == 4 && out[7] == -4);
  src->Push(7, 5); // wraps: ring positions 6,7,0,1,2
  CHECK(cap.QueuedFrames() == 7);
  CHECK(cap.Read(out, 32) == 7);
  for (int i = 0; i < 7; ++i) {
    CHECK(out[2 * i] == 5 + i);
    CHECK(out[2 * i + 1] == -(5 + i));
  }
  CHECK(cap.Read(out, 32) == 0);
  CHECK(cap.OverrunBlocks() == 0);
  cap.Close();
  CHECK(cap.GetState() == HostAudioCapture::State::Closed && !src->open_);
}

void Overrun() {
  FakeSource *src;
  HostAudioCapture cap = Make(src, true, 8);
  CHECK(cap.Open());
  src->Push(1, 6);
  src->Push(100, 4); // only 2 free: whole block dropped
  CHECK(cap.OverrunBlocks() == 1 && cap.OverrunFrames() == 4);
  CHECK(cap.QueuedFrames() == 6);
  int16_t out[32];
  CHECK(cap.Read(out, 32) == 6);
  CHECK(out[10] == 6); // kept data is the first block, uncorrupted
  src->Push(100, 8); // exactly full is fine
  CHECK(cap.OverrunBlocks() == 1 && cap.QueuedFrames() == 8);
  src->Push(1, 1);
  CHECK(cap.OverrunBlocks() == 2 && cap.OverrunFrames() == 5);
}

void Level() {
  FakeSource *src;
  HostAudioCapture cap = Make(src, true, 16);
  CHECK(cap.Open());
  CHECK(cap.Level() == 0);
  src->Push(-300, 2); // samples -300, -299 / 300, 299
  CHECK(cap.Level() == 300);
  cap.Close();
  CHECK(cap.Level() == 0);
}

void Unavailable() {
  FakeSource *src;
  HostAudioCapture cap = Make(src, false, 8);
  CHECK(!cap.Open());
  CHECK(cap.GetState() == HostAudioCapture::State::Unavailable);
  CHECK(!cap.Available());
  int16_t out[4];
  CHECK(cap.Read(out, 2) == 0 && cap.Level() == 0);
  cap.Close(); // harmless
  CHECK(cap.GetState() == HostAudioCapture::State::Unavailable);

  HostAudioCapture none(nullptr, 8);
  CHECK(!none.Open() && none.Read(out, 2) == 0);
  HostAudioCapture empty(std::make_unique<FakeSource>(true), 0);
  CHECK(!empty.Open());
}

void DummyDriver() {
  SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
  HostAudioCapture cap(MakeSDLCaptureSource());
  CHECK(cap.Open());
  std::vector<int16_t> out(4096 * 2, 1);
  size_t got = 0;
  for (int i = 0; i < 100 && got == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    got = cap.Read(out.data(), 4096);
  }
  CHECK(got > 0);
  for (size_t i = 0; i < got * 2; ++i)
    CHECK(out[i] == 0);
  CHECK(cap.Level() == 0);
  cap.Close();
  CHECK(!cap.Available());
  CHECK(cap.Open()); // reopen works
  cap.Close();
}

} // namespace

int main() {
  RingWrap();
  Overrun();
  Level();
  Unavailable();
  DummyDriver();
  std::puts("host_audio_capture_check passed");
  return 0;
}
