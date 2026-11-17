// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

// Checks HostRecord (the firmware's record.h) against a fake capture source:
// WAV round trip through the sample pool, threshold start, cancel, limits and
// gain clamping, unsupported sources.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

#include "HostAudioCapture.h"
#include "HostFileSystem.h"
#include "HostRecord.h"
#include "HostSamplePool.h"

#include <Adapters/adv/audio/record.h>
#if defined(PICOTRACKER_LINUX_BACKEND)
#include "LinuxBackend.h"
#endif

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace fs = std::filesystem;

namespace {

class FakeSource : public HostCaptureSource {
public:
  explicit FakeSource(bool opens = true) : opens_(opens) {}
  bool Open(HostCaptureSink *sink) override {
    if (!opens_) return false;
    sink_ = sink;
    return true;
  }
  void Close() override {}
  HostCaptureSink *sink_ = nullptr;

private:
  bool opens_;
};

struct Rig {
  FakeSource *source = nullptr;
  HostAudioCapture *capture = nullptr;
};

Rig Install(bool opens = true) {
  auto src = std::make_unique<FakeSource>(opens);
  Rig rig;
  rig.source = src.get();
  auto cap = std::make_unique<HostAudioCapture>(std::move(src), 8192);
  rig.capture = cap.get();
  HostRecordSetCapture(std::move(cap));
  return rig;
}

template <class Pred> bool WaitFor(Pred pred, int ms = 5000) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < end) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

// Pushes frames to the capture without overrunning the ring.
void Push(Rig &rig, const std::vector<int16_t> &interleaved) {
  const size_t frames = interleaved.size() / 2;
  for (size_t at = 0; at < frames;) {
    const size_t n = std::min<size_t>(512, frames - at);
    CHECK(WaitFor([&] { return rig.capture->QueuedFrames() + n <= 8192; }));
    rig.source->sink_->OnCaptureBlock(interleaved.data() + at * 2, n);
    at += n;
  }
  // Let the worker take everything queued.
  CHECK(WaitFor([&] { return rig.capture->QueuedFrames() == 0; }));
}

std::vector<int16_t> Ramp(int first, size_t frames) {
  std::vector<int16_t> v(frames * 2);
  for (size_t i = 0; i < frames; ++i) {
    v[2 * i] = static_cast<int16_t>(first + static_cast<int>(i % 20000));
    v[2 * i + 1] = static_cast<int16_t>(-(first + static_cast<int>(i % 20000)));
  }
  return v;
}

void StopAndFinish() {
  RequestStopRecording();
  CHECK(WaitForRecordingStop(5000));
  FinishStopRecording();
}

std::vector<uint8_t> ReadAll(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  CHECK(in.good());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}

constexpr size_t kHeader = 44;

// File wrapper that blocks the second 4096-byte write until released, so a
// check can observe and interrupt a save in progress.
class GateFs : public FileSystem {
public:
  explicit GateFs(HostFileSystem &inner) : inner_(inner) {}
  FileHandle Open(const char *name, const char *mode) override;
  bool ListDirectory(const char *d, etl::ivector<FileSystemEntry> *e, const char *f,
                     bool dirs, bool hidden = false) override {
    return inner_.ListDirectory(d, e, f, dirs, hidden);
  }
  bool DeleteFile(const char *n) override { return inner_.DeleteFile(n); }
  bool DeleteDir(const char *n) override { return inner_.DeleteDir(n); }
  bool exists(const char *p) override { return inner_.exists(p); }
  bool makeDir(const char *p, bool parents = false) override { return inner_.makeDir(p, parents); }
  bool CopyFile(const char *a, const char *b) override { return inner_.CopyFile(a, b); }
  bool MoveFile(const char *a, const char *b) override { return inner_.MoveFile(a, b); }
  bool isExFat() override { return false; }

  void Arm() { gated_ = true; blocked_ = false; released_ = false; chunks_ = 0; }
  bool Blocked() const { return blocked_; }
  void Release() {
    std::lock_guard<std::mutex> l(m_);
    released_ = true;
    cv_.notify_all();
  }
  // Called by the file wrapper for each 4096-byte write.
  void OnChunk() {
    if (!gated_ || ++chunks_ < 2) return;
    std::unique_lock<std::mutex> l(m_);
    blocked_ = true;
    cv_.wait(l, [&] { return released_; });
  }

private:
  HostFileSystem &inner_;
  std::atomic<bool> gated_{false}, blocked_{false};
  bool released_ = false;
  int chunks_ = 0;
  std::mutex m_;
  std::condition_variable cv_;
};

class GateFile : public I_File {
public:
  GateFile(FileHandle inner, GateFs *fs) : inner_(std::move(inner)), fs_(fs) {}
  int Read(void *p, int s) override { return inner_->Read(p, s); }
  int GetC() override { return inner_->GetC(); }
  int Write(const void *p, int size, int n) override {
    if (size == 4096) fs_->OnChunk();
    return inner_->Write(p, size, n);
  }
  void Seek(long o, int w) override { inner_->Seek(o, w); }
  long Tell() override { return inner_->Tell(); }
  int Error() override { return inner_->Error(); }
  bool Sync() override { return inner_->Sync(); }
  void Dispose() override { delete this; }

private:
  bool Close() override { inner_.reset(); return true; }
  FileHandle inner_;
  GateFs *fs_;
};

FileHandle GateFs::Open(const char *name, const char *mode) {
  FileHandle h = inner_.Open(name, mode);
  if (!h) return h;
  return FileHandle(new GateFile(std::move(h), this));
}

fs::path dataDir;

void RoundTrip() {
  Rig rig = Install();
  SetInputSource(LineIn);
  SetLineInGain(0);
  CHECK(!IsRecordingActive() && !IsSavingRecording());
  CHECK(StartRecording("/rec/ramp.wav", 0, 0));
  CHECK(IsRecordingActive());
  const auto signal = Ramp(1, 30000);
  Push(rig, signal);
  StopAndFinish();
  CHECK(!IsRecordingActive() && !IsSavingRecording());
  CHECK(DidLastRecordingCaptureAudio());
  CHECK(GetSavingProgressPercent() == 100);

  const auto bytes = ReadAll(dataDir / "rec/ramp.wav");
  CHECK(bytes.size() == kHeader + signal.size() * 2);
  CHECK(std::equal(bytes.begin() + kHeader, bytes.end(),
                   reinterpret_cast<const uint8_t *>(signal.data())));

  HostSamplePool pool;
  CHECK(pool.LoadProjectSample("/rec/ramp.wav") == 0);
  auto *source = pool.GetSource(0);
  CHECK(source);
  CHECK(source->GetSampleRate(0) == 44100);
  CHECK(source->GetSize(0) == static_cast<int>(signal.size() / 2));
  CHECK(std::memcmp(source->GetSampleBuffer(0), signal.data(), signal.size() * 2) == 0);
  pool.Reset();
}

void Mono() {
  Rig rig = Install();
  SetInputSource(Mic);
  SetMicGain(0);
  CHECK(StartRecording("/rec/mic.wav", 0, 0));
  const auto signal = Ramp(7, 2000);
  Push(rig, signal);
  StopAndFinish();
  CHECK(DidLastRecordingCaptureAudio());
  const auto bytes = ReadAll(dataDir / "rec/mic.wav");
  CHECK(bytes.size() == kHeader + 2000 * 2);
  const auto *s = reinterpret_cast<const int16_t *>(bytes.data() + kHeader);
  for (size_t i = 0; i < 2000; ++i) CHECK(s[i] == signal[2 * i]);
  SetInputSource(LineIn);
}

void LineInMono() {
  Rig rig = Install();
  SetInputSource(LineIn);
  setenv("PICOTRACKER_LINEIN_MONO", "1", 1);
  const auto signal = Ramp(1, 30000);
  CHECK(StartRecording("/rec/linemono.wav", 0, 0));
  Push(rig, signal);
  StopAndFinish();
  unsetenv("PICOTRACKER_LINEIN_MONO");
  const auto bytes = ReadAll(dataDir / "rec/linemono.wav");
  CHECK(bytes.size() == kHeader + signal.size() * 2);
  const auto *s = reinterpret_cast<const int16_t *>(bytes.data() + kHeader);
  for (size_t i = 0; i < signal.size() / 2; ++i) {
    CHECK(s[2 * i] == signal[2 * i]);
    CHECK(s[2 * i + 1] == signal[2 * i]);
  }
}

void Threshold() {
  Rig rig = Install();
  SetInputSource(LineIn);
  // threshold 10 is a level of 1280; quiet frames before it are dropped.
  std::vector<int16_t> input(2000 * 2, 100);
  const auto loud = Ramp(5000, 1000);
  input.insert(input.end(), loud.begin(), loud.end());
  CHECK(StartRecording("/rec/thr.wav", 10, 0));
  Push(rig, input);
  StopAndFinish();
  CHECK(DidLastRecordingCaptureAudio());
  const auto bytes = ReadAll(dataDir / "rec/thr.wav");
  CHECK(bytes.size() == kHeader + loud.size() * 2);
  CHECK(std::memcmp(bytes.data() + kHeader, loud.data(), loud.size() * 2) == 0);

  // Only silence: nothing captured, no file.
  CHECK(StartRecording("/rec/quiet.wav", 10, 0));
  Push(rig, std::vector<int16_t>(3000 * 2, 100));
  StopAndFinish();
  CHECK(!DidLastRecordingCaptureAudio());
  CHECK(!fs::exists(dataDir / "rec/quiet.wav"));
}

void MaxDuration() {
  Rig rig = Install();
  CHECK(StartRecording("/rec/short.wav", 0, 10)); // 10 ms = 441 frames
  const auto input = Ramp(1, 2000); // the worker stops reading at 441 frames
  rig.source->sink_->OnCaptureBlock(input.data(), 2000);
  CHECK(WaitForRecordingStop(5000));
  CHECK(!IsRecordingActive()); // stopped on its own
  FinishStopRecording();
  CHECK(DidLastRecordingCaptureAudio());
  CHECK(ReadAll(dataDir / "rec/short.wav").size() == kHeader + 441 * 4);
}

void CancelBeforeSave() {
  Rig rig = Install();
  CHECK(StartRecording("/rec/cancel.wav", 0, 0));
  Push(rig, Ramp(1, 4000));
  CancelRecording();
  CHECK(!IsRecordingActive() && !IsSavingRecording());
  CHECK(!DidLastRecordingCaptureAudio());
  CHECK(!fs::exists(dataDir / "rec/cancel.wav"));
  // A fresh recording works afterwards.
  CHECK(StartRecording("/rec/after.wav", 0, 0));
  Push(rig, Ramp(1, 100));
  StopAndFinish();
  CHECK(DidLastRecordingCaptureAudio());
}

void CancelDuringSave(GateFs &gate) {
  Rig rig = Install();
  gate.Arm();
  CHECK(StartRecording("/rec/partial.wav", 0, 0));
  Push(rig, Ramp(1, 6000)); // 24000 bytes, several chunks
  RequestStopRecording();
  CHECK(WaitFor([&] { return gate.Blocked(); }));
  CHECK(IsSavingRecording());
  CHECK(!IsRecordingActive());
  CHECK(GetSavingProgressPercent() < 100);
  CHECK(fs::exists(dataDir / "rec/partial.wav")); // partial file is on disk
  std::thread canceller(CancelRecording);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  gate.Release();
  canceller.join();
  CHECK(!IsSavingRecording());
  CHECK(!DidLastRecordingCaptureAudio());
  CHECK(!fs::exists(dataDir / "rec/partial.wav"));
}

void SaveProgress(GateFs &gate) {
  Rig rig = Install();
  gate.Arm();
  CHECK(StartRecording("/rec/progress.wav", 0, 0));
  Push(rig, Ramp(1, 6000));
  RequestStopRecording();
  CHECK(WaitFor([&] { return gate.Blocked(); }));
  const uint8_t mid = GetSavingProgressPercent();
  CHECK(IsSavingRecording() && mid > 0 && mid < 100);
  CHECK(!WaitForRecordingStop(0));
  gate.Release();
  CHECK(WaitForRecordingStop(5000));
  FinishStopRecording();
  CHECK(GetSavingProgressPercent() == 100 && DidLastRecordingCaptureAudio());
}

void GainClamp() {
  SetLineInGain(100);
  CHECK(HostRecordLineInGainDb() == LINEIN_GAIN_MAXDB);
  SetLineInGain(-100);
  CHECK(HostRecordLineInGainDb() == LINEIN_GAIN_MINDB);
  SetLineInGain(3);
  CHECK(HostRecordLineInGainDb() == 3);
  SetMicGain(100);
  CHECK(HostRecordMicGainDb() == MIC_GAIN_MAXDB);
  SetMicGain(-5);
  CHECK(HostRecordMicGainDb() == MIC_GAIN_MINDB);

  // +6 dB is about double and saturates at full scale.
  Rig rig = Install();
  SetInputSource(LineIn);
  SetLineInGain(6);
  CHECK(StartRecording("/rec/gain.wav", 0, 0));
  std::vector<int16_t> input = {1000, -1000, 30000, -30000};
  Push(rig, input);
  StopAndFinish();
  const auto bytes = ReadAll(dataDir / "rec/gain.wav");
  const auto *s = reinterpret_cast<const int16_t *>(bytes.data() + kHeader);
  CHECK(s[0] > 1990 && s[0] < 2010 && s[1] == -s[0]);
  CHECK(s[2] == 32767 && s[3] == -32768);
  SetLineInGain(0);
}

void Unsupported() {
  Rig rig = Install();
  SetInputSource(USBIn);
  CHECK(!StartRecording("/rec/usb.wav", 0, 0));
  StartMonitoring();
  CHECK(!IsRecordingActive());
  SetInputSource(AllOff);
  CHECK(!StartRecording("/rec/off.wav", 0, 0));
  SetInputSource(LineIn);
  CHECK(!StartRecording(nullptr, 0, 0));
  CHECK(!StartRecording("relative.wav", 0, 0));
  CHECK(!fs::exists(dataDir / "rec/usb.wav"));

  Install(false); // capture device cannot open
  CHECK(!StartRecording("/rec/none.wav", 0, 0));
  CHECK(!IsRecordingActive());
  StartMonitoring();
  CHECK(!IsRecordingActive());
  CHECK(WaitForRecordingStop(0)); // nothing pending
}

void Monitoring() {
  Rig rig = Install();
  SetInputSource(LineIn);
  StartMonitoring();
  CHECK(IsRecordingActive());
  Push(rig, Ramp(1, 3000)); // monitored audio is discarded, not recorded
  CHECK(StartRecording("/rec/mon.wav", 0, 0));
  const auto signal = Ramp(100, 500);
  Push(rig, signal);
  StopAndFinish();
  const auto bytes = ReadAll(dataDir / "rec/mon.wav");
  CHECK(bytes.size() == kHeader + signal.size() * 2);
  StartMonitoring();
  StopMonitoring();
  CHECK(!IsRecordingActive());
}

#if defined(PICOTRACKER_LINUX_BACKEND)
// recordingAvailable follows whether capture really opened.
void LinuxBackendReportsCapture() {
  AudioSettings settings;
  auto status = [&](bool enabled, bool opens) {
    LinuxBackendOptions options;
    options.captureEnabled = enabled;
    options.captureSource = [opens] {
      return std::make_unique<FakeSource>(opens);
    };
    LinuxBackend backend(settings, std::move(options));
    backend.Install();
    return backend.Status();
  };
  PlatformStatus ok = status(true, true);
  CHECK(ok.recordingAvailable);
  CHECK(ok.recordingError.empty());
  PlatformStatus failed = status(true, false);
  CHECK(!failed.recordingAvailable);
  CHECK(!failed.recordingError.empty());
  SetInputSource(LineIn);
  // With the capture failing, recording refuses to start instead of crashing.
  CHECK(!StartRecording("/rec/never.wav", 0, 0));
  PlatformStatus off = status(false, true);
  CHECK(!off.recordingAvailable);
  CHECK(!StartRecording("/rec/never.wav", 0, 0));
}
#endif

} // namespace

int main() {
  dataDir = fs::temp_directory_path() / ("host_record_check_" + std::to_string(::getpid()));
  fs::remove_all(dataDir);
  HostFileSystem hostFs(dataDir);
  CHECK(hostFs.valid());
  CHECK(hostFs.makeDir("/rec"));
  GateFs gate(hostFs);
  FileSystem::Install(&gate);

  RoundTrip();
  Mono();
  LineInMono();
  Threshold();
  MaxDuration();
  CancelBeforeSave();
  CancelDuringSave(gate);
  SaveProgress(gate);
  GainClamp();
  Unsupported();
  Monitoring();
#if defined(PICOTRACKER_LINUX_BACKEND)
  LinuxBackendReportsCapture();
#endif

  HostRecordSetCapture(nullptr);
  fs::remove_all(dataDir);
  std::puts("host_record_check passed");
  return 0;
}
