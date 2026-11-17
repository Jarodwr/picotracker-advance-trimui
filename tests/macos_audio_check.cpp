// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

// Real-device audio check for the production SDLAudioDriver on macOS.
//
// Drives the driver exactly as the firmware does (Init/Start, Pump from the
// firmware thread, buffers delivered through the upstream AudioDriver pool)
// with a repeatable 440 Hz signal, and exercises: non-silent output on the
// default device, start/stop cycles, a timed run (default 60 s) comparing the
// stream clock with wall time, and device loss.
//
// Build (from the repository root, SDL2 from Homebrew):
//   U=upstream/advance-firmware/sources
//   c++ -std=c++23 -DADV -Dstrlen=__builtin_strlen "-Dsection(x)=unused" \
//     -Wno-deprecated-declarations -Iplatform/desktop -Iplatform/desktop/include \
//     -I$U -I$U/config -I$U/Externals/etl/include -I$U/Foundation/Variables \
//     $(sdl2-config --cflags) tests/macos_audio_check.cpp \
//     platform/desktop/SDLAudioDriver.cpp platform/desktop/HostTrace.cpp \
//     $U/Services/Audio/AudioDriver.cpp $U/Foundation/Observable.cpp \
//     -Wl,-dead_strip $(sdl2-config --libs) -o macos_audio_check
// (-dead_strip drops MakeSDLAudioOut, the only user of the upstream output
// chain, so the mixer and sync code need not be linked.)
// Run: ./macos_audio_check [seconds]   (default 60; PICOTRACKER_AUDIO_DEVICE
// selects a named SDL device). Exit status is non-zero if any check fails.
//
// Limits: the check cannot hear the speaker. "Non-silent" means a non-zero
// signal was handed to an open CoreAudio device and the device consumed it at
// real time; audibility is confirmed by ear (the tone is announced on stdout).

#include "SDLAudioDriver.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

int failures = 0;

void check(bool ok, const char *what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok)
    ++failures;
}

double secondsSince(Clock::time_point t) {
  return std::chrono::duration<double>(Clock::now() - t).count();
}

void sleepMs(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Plays a continuous sine whenever the driver asks for a buffer.
class ToneSource final : public I_Observer {
public:
  explicit ToneSource(SDLAudioDriver &d) : driver_(d) { d.AddObserver(*this); }
  ~ToneSource() override { driver_.RemoveObserver(*this); }

  void Update(Observable &, I_ObservableData *data) override {
    auto *event = static_cast<AudioDriver::Event *>(data);
    if (event->type_ == AudioDriver::Event::ADET_DRIVERTICK) {
      ++ticks;
      return;
    }
    short buffer[kChunkFrames * 2];
    for (int i = 0; i < kChunkFrames; ++i) {
      const short s = static_cast<short>(
          std::lround(8000.0 * std::sin(phase_)));
      buffer[2 * i] = buffer[2 * i + 1] = s;
      peak = std::max<int>(peak, std::abs(s));
      phase_ += 2.0 * M_PI * 440.0 / SDLAudioDriver::kSampleRate;
    }
    if (phase_ > 2.0 * M_PI)
      phase_ = std::fmod(phase_, 2.0 * M_PI);
    driver_.AddBuffer(buffer, kChunkFrames);
    frames += kChunkFrames;
  }

  static constexpr int kChunkFrames = 735; // one 60 Hz frame
  long ticks = 0;
  long long frames = 0;
  int peak = 0;

private:
  SDLAudioDriver &driver_;
  double phase_ = 0;
};

// Pumps like the firmware event loop for `seconds`, returns pump gap stats.
struct RunStats {
  double maxGapMs = 0;
  long starved = 0; // polls that saw the device queue empty
  long polls = 0;
  double wall = 0;
  double stream = 0;
};

RunStats run(SDLAudioDriver &driver, double seconds) {
  RunStats stats;
  const auto start = Clock::now();
  auto last = start;
  while (secondsSince(start) < seconds) {
    driver.Pump();
    ++stats.polls;
    if (driver.GetPlayedBufferPercentage() >= 100)
      ++stats.starved;
    const auto now = Clock::now();
    stats.maxGapMs = std::max(
        stats.maxGapMs, std::chrono::duration<double, std::milli>(now - last).count());
    last = now;
    sleepMs(2);
  }
  stats.wall = secondsSince(start);
  stats.stream = driver.GetStreamTime();
  return stats;
}

} // namespace

int main(int argc, char **argv) {
  const double timedSeconds = argc > 1 ? std::atof(argv[1]) : 60.0;
  const char *deviceEnv = std::getenv("PICOTRACKER_AUDIO_DEVICE");

  AudioSettings settings;
  settings.bufferSize_ = 1024;
  settings.preBufferCount_ = 0;
  if (deviceEnv)
    settings.audioDevice_ = deviceEnv;

  auto *driver = new SDLAudioDriver(settings);
  auto toneHolder = std::make_unique<ToneSource>(*driver);
  ToneSource &tone = *toneHolder;

  std::printf("1. open default device\n");
  check(driver->Init(), "Init opens an output device");
  const SDLAudioStatus opened = driver->GetStatus();
  std::printf("  device=\"%s\" api=%s rate=%d channels=%d bufferFrames=%d\n",
              opened.deviceName.c_str(),
              SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?",
              opened.sampleRate, opened.channels, opened.bufferFrames);
  std::printf("SDL audio driver list:");
  for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
    std::printf(" %s", SDL_GetAudioDriver(i));
  std::printf("\nOutput devices:");
  for (int i = 0; i < SDL_GetNumAudioDevices(0); ++i)
    std::printf(" \"%s\"", SDL_GetAudioDeviceName(i, 0));
  std::printf("\n");
  check(opened.deviceOpen && !opened.silentFallback, "real device, not silent fallback");
  if (!opened.deviceOpen) {
    std::printf("No usable output device; remaining checks skipped.\n");
    delete driver;
    return 1;
  }

  std::printf("2. non-silent signal (440 Hz sine, ~3 s: you should hear a tone)\n");
  check(driver->Start(), "Start");
  RunStats s = run(*driver, 3.0);
  check(tone.peak > 0, "signal handed to the driver has non-zero peak");
  std::printf("  peak=%d frames submitted=%lld ticks=%ld\n", tone.peak,
              tone.frames, tone.ticks);
  check(std::fabs(s.stream - s.wall) < 0.25,
        "device consumed audio at real time (stream clock within 250 ms of wall)");
  std::printf("  wall=%.3f stream=%.3f maxPumpGap=%.1fms starved=%ld/%ld\n",
              s.wall, s.stream, s.maxGapMs, s.starved, s.polls);

  std::printf("3. start/stop cycles\n");
  bool cyclesOk = true;
  for (int i = 0; i < 5; ++i) {
    driver->Stop();
    cyclesOk &= driver->GetStreamTime() == 0.0;
    cyclesOk &= driver->Pump() == 0; // not playing: no work
    cyclesOk &= driver->Start();
    RunStats r = run(*driver, 0.5);
    cyclesOk &= std::fabs(r.stream - r.wall) < 0.25;
    std::printf("  cycle %d: wall=%.3f stream=%.3f\n", i + 1, r.wall, r.stream);
  }
  check(cyclesOk, "5 stop/start cycles: clock resets, resumes, stays in step");
  driver->Stop();
  check(driver->GetStatus().deviceOpen, "device still open after Stop");

  std::printf("4. timed run: %.0f s\n", timedSeconds);
  check(driver->Start(), "Start");
  const auto start = Clock::now();
  double maxDrift = 0;
  RunStats total;
  long starved = 0, polls = 0;
  for (int sec = 1; sec <= static_cast<int>(timedSeconds); ++sec) {
    const double slice = sec - secondsSince(start);
    RunStats r = run(*driver, slice > 0 ? slice : 0);
    starved += r.starved;
    polls += r.polls;
    total.maxGapMs = std::max(total.maxGapMs, r.maxGapMs);
    const double drift = driver->GetStreamTime() - secondsSince(start);
    maxDrift = std::max(maxDrift, std::fabs(drift));
    if (sec % 10 == 0)
      std::printf("  t=%2ds drift=%+.1f ms\n", sec, drift * 1000.0);
  }
  const double wall = secondsSince(start);
  const double stream = driver->GetStreamTime();
  const double driftMs = (stream - wall) * 1000.0;
  std::printf("  wall=%.3f stream=%.3f finalDrift=%+.1fms maxAbsDrift=%.1fms "
              "maxPumpGap=%.1fms starved=%ld/%ld\n",
              wall, stream, driftMs, maxDrift * 1000.0, total.maxGapMs, starved, polls);
  check(maxDrift < 0.25, "stream clock never more than 250 ms from wall time");
  check(starved == 0, "device queue never ran empty");
  check(!driver->GetStatus().deviceLost, "no spurious device loss");

  std::printf("5. device loss\n");
  // Close the device behind the driver's back, as the OS does on removal:
  // the driver sees SDL_AUDIO_STOPPED and must fall back to a silent clock.
  int closed = 0;
  for (SDL_AudioDeviceID id = 2; id < 12; ++id) {
    if (SDL_GetAudioDeviceStatus(id) != SDL_AUDIO_STOPPED) {
      SDL_CloseAudioDevice(id);
      ++closed;
    }
  }
  check(closed == 1, "found and closed exactly one open device");
  const double before = driver->GetStreamTime();
  run(*driver, 1.0);
  const SDLAudioStatus lost = driver->GetStatus();
  check(lost.deviceLost && lost.silentFallback && !lost.deviceOpen,
        "loss detected, silent fallback entered");
  std::printf("  error=\"%s\"\n", lost.error.c_str());
  check(driver->GetStreamTime() > before + 0.5,
        "stream clock keeps advancing without a device");
  driver->Stop();
  check(driver->Start(), "Stop/Start still succeed after loss");
  run(*driver, 0.5);
  check(driver->GetStreamTime() > 0.25, "clock runs after restart in fallback");
  driver->Stop();
  driver->Close();
  check(!driver->GetStatus().deviceOpen, "Close after loss is clean");

  std::printf("6. reopen after loss\n");
  check(driver->Init() && driver->GetStatus().deviceOpen && !driver->GetStatus().silentFallback,
        "Init reopens a real device");
  check(driver->Start(), "Start");
  s = run(*driver, 1.0);
  check(std::fabs(s.stream - s.wall) < 0.25, "audio flows again");
  driver->Stop();
  driver->Close();

  toneHolder.reset(); // detach before the driver goes away
  delete driver;
  std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures,
              failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
