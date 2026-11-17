// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostRecord.h"

#include <Adapters/adv/audio/record.h>

#include "Application/Instruments/WavHeader.h"
#include "System/FileSystem/FileSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// Same limits as the firmware's record RAM buffer (record.cpp).
constexpr uint32_t kMaxRecordSeconds = 30;
constexpr uint32_t kSaveChunkBytes = 4096;
constexpr uint32_t kStopTimeoutMs = 30000;
constexpr size_t kBlockFrames = 1024;
// threshold is 0..255 of a 32768 full scale; 0 starts immediately.
constexpr int kThresholdUnit = 128;

enum class Phase { Idle, Monitoring, Capturing, Saving };

class Recorder {
public:
  ~Recorder() { Shutdown(); }

  void SetCapture(std::unique_ptr<HostAudioCapture> capture) {
    Shutdown();
    std::lock_guard<std::mutex> lock(m_);
    capture_ = std::move(capture);
  }

  bool Start(const char *filename, uint8_t threshold, uint32_t milliseconds) {
    if (!filename || filename[0] != '/') return false;
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&] { return phase_ != Phase::Saving; });
    if (phase_ == Phase::Capturing || !SourceSupported()) return false;
    if (!EnsureCaptureOpen()) return false;
    if (phase_ == Phase::Monitoring) {
      // Drop what was monitored before the record press.
      DrainDiscard();
    } else {
      JoinWorker(lk);
    }
    path_ = filename;
    channels_ = source_ == Mic ? 1 : 2;
    const char *mono = std::getenv("PICOTRACKER_LINEIN_MONO");
    monoLineIn_ = mono && mono[0] == '1';
    thresholdLevel_ = static_cast<int>(threshold) * kThresholdUnit;
    triggered_ = thresholdLevel_ == 0;
    const uint64_t maxFrames = static_cast<uint64_t>(kMaxRecordSeconds) * kCaptureSampleRate;
    maxFrames_ = milliseconds == 0
                     ? maxFrames
                     : std::min<uint64_t>(
                           maxFrames, static_cast<uint64_t>(milliseconds) *
                                          kCaptureSampleRate / 1000);
    samples_.clear();
    samples_.reserve(static_cast<size_t>(maxFrames_) * channels_);
    stopRequested_ = false;
    cancelRequested_ = false;
    savedAudio_ = false;
    percent_ = 0;
    sessionPending_ = true;
    const bool needWorker = phase_ != Phase::Monitoring;
    phase_ = Phase::Capturing;
    if (needWorker) StartWorker();
    return true;
  }

  void StartMonitoring() {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&] { return phase_ != Phase::Saving; });
    if (phase_ != Phase::Idle || !SourceSupported() || !EnsureCaptureOpen())
      return;
    JoinWorker(lk);
    phase_ = Phase::Monitoring;
    StartWorker();
  }

  void StopMonitoring() {
    std::unique_lock<std::mutex> lk(m_);
    if (phase_ != Phase::Monitoring) return;
    phase_ = Phase::Idle;
    JoinWorker(lk);
    CloseCapture();
  }

  void RequestStop() {
    std::lock_guard<std::mutex> lock(m_);
    if (phase_ == Phase::Capturing) stopRequested_ = true;
    else if (phase_ == Phase::Monitoring) phase_ = Phase::Idle;
  }

  bool WaitForStop(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lk(m_);
    auto done = [&] { return !sessionPending_; };
    if (timeoutMs == 0x7FFFFFFFU) {
      cv_.wait(lk, done);
      return true;
    }
    return cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), done);
  }

  void Finish() {
    std::unique_lock<std::mutex> lk(m_);
    if (!sessionPending_ && phase_ == Phase::Idle) JoinWorker(lk);
  }

  void Stop() {
    RequestStop();
    if (!WaitForStop(kStopTimeoutMs)) return;
    Finish();
  }

  void Cancel() {
    {
      std::lock_guard<std::mutex> lock(m_);
      if (phase_ != Phase::Capturing && phase_ != Phase::Saving) return;
      cancelRequested_ = true;
      stopRequested_ = true;
    }
    WaitForStop(kStopTimeoutMs);
    Finish();
  }

  void SetSource(RecordSource source) {
    std::lock_guard<std::mutex> lock(m_);
    source_ = source;
  }

  void SetLineGain(int32_t db) {
    std::lock_guard<std::mutex> lock(m_);
    lineGainDb_ = std::clamp<int32_t>(db, LINEIN_GAIN_MINDB, LINEIN_GAIN_MAXDB);
  }

  void SetMicGain(int32_t db) {
    std::lock_guard<std::mutex> lock(m_);
    micGainDb_ = std::clamp<int32_t>(db, MIC_GAIN_MINDB, MIC_GAIN_MAXDB);
  }

  int32_t LineGain() {
    std::lock_guard<std::mutex> lock(m_);
    return lineGainDb_;
  }
  int32_t MicGain() {
    std::lock_guard<std::mutex> lock(m_);
    return micGainDb_;
  }

  bool Active() {
    std::lock_guard<std::mutex> lock(m_);
    return phase_ == Phase::Monitoring || phase_ == Phase::Capturing;
  }
  bool Saving() {
    std::lock_guard<std::mutex> lock(m_);
    return phase_ == Phase::Saving;
  }
  uint8_t Percent() {
    std::lock_guard<std::mutex> lock(m_);
    return percent_;
  }
  bool SavedAudio() {
    std::lock_guard<std::mutex> lock(m_);
    return savedAudio_;
  }

private:
  bool SourceSupported() const { return source_ == LineIn || source_ == Mic; }

  bool EnsureCaptureOpen() {
    if (!capture_) capture_ = std::make_unique<HostAudioCapture>(MakeSDLCaptureSource());
    return capture_->Available() || capture_->Open();
  }

  void CloseCapture() {
    if (capture_) capture_->Close();
  }

  void DrainDiscard() {
    int16_t scratch[kBlockFrames * kCaptureChannels];
    while (capture_->Read(scratch, kBlockFrames) > 0) {
    }
  }

  void StartWorker() { worker_ = std::thread([this] { Run(); }); }

  // Called with lk held; the worker needs m_ to wind down, so release it.
  void JoinWorker(std::unique_lock<std::mutex> &lk) {
    if (!worker_.joinable()) return;
    std::thread t = std::move(worker_);
    lk.unlock();
    t.join();
    lk.lock();
  }

  void Shutdown() {
    {
      std::unique_lock<std::mutex> lk(m_);
      if (phase_ == Phase::Capturing || phase_ == Phase::Saving) {
        cancelRequested_ = true;
        stopRequested_ = true;
      } else if (phase_ == Phase::Monitoring) {
        phase_ = Phase::Idle;
      }
      JoinWorker(lk);
      CloseCapture();
    }
  }

  float GainFactor() const {
    const int32_t db = source_ == Mic ? micGainDb_ : lineGainDb_;
    return db == 0 ? 1.0f : std::pow(10.0f, static_cast<float>(db) / 20.0f);
  }

  // Appends the frames of one block that belong to the recording. Returns
  // false when the recording is full.
  bool Consume(const int16_t *block, size_t frames, float gain) {
    for (size_t i = 0; i < frames; ++i) {
      const int16_t left = block[2 * i];
      // A one-mic device (Trimui Smart Pro, MIC1 on the left) leaves the
      // right Line In channel as noise: duplicate L to R instead.
      const int16_t right = monoLineIn_ ? left : block[2 * i + 1];
      if (!triggered_) {
        const int peak = std::max(std::abs(static_cast<int>(left)),
                                  channels_ == 1 ? 0 : std::abs(static_cast<int>(right)));
        if (peak < thresholdLevel_) continue;
        triggered_ = true;
      }
      if (samples_.size() / channels_ >= maxFrames_) return false;
      samples_.push_back(Apply(left, gain));
      if (channels_ == 2) samples_.push_back(Apply(right, gain));
    }
    return samples_.size() / channels_ < maxFrames_;
  }

  static int16_t Apply(int16_t sample, float gain) {
    if (gain == 1.0f) return sample;
    const float v = std::round(static_cast<float>(sample) * gain);
    return static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
  }

  void Run() {
    std::vector<int16_t> block(kBlockFrames * kCaptureChannels);
    std::unique_lock<std::mutex> lk(m_);
    for (;;) {
      if (phase_ == Phase::Idle) return;
      if (phase_ == Phase::Capturing && stopRequested_) break;
      const size_t frames = capture_->Read(block.data(), kBlockFrames);
      if (frames == 0) {
        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        lk.lock();
        continue;
      }
      if (phase_ == Phase::Capturing &&
          !Consume(block.data(), frames, GainFactor()))
        stopRequested_ = true;
    }
    Finalize(lk);
  }

  // phase_ == Capturing and a stop was requested; lk held on entry and exit.
  void Finalize(std::unique_lock<std::mutex> &lk) {
    const bool cancelled = cancelRequested_;
    if (cancelled || samples_.empty()) {
      savedAudio_ = false;
    } else {
      phase_ = Phase::Saving;
      percent_ = 0;
      const std::string path = path_;
      const uint16_t channels = channels_;
      lk.unlock();
      const bool saved = Save(path, channels);
      lk.lock();
      savedAudio_ = saved;
      if (saved) percent_ = 100;
    }
    samples_.clear();
    samples_.shrink_to_fit();
    CloseCapture();
    phase_ = Phase::Idle;
    sessionPending_ = false;
    cv_.notify_all();
  }

  bool Cancelled() {
    std::lock_guard<std::mutex> lock(m_);
    return cancelRequested_;
  }

  void SetPercent(uint8_t percent) {
    std::lock_guard<std::mutex> lock(m_);
    percent_ = percent;
  }

  // Same byte layout as the firmware (record.cpp PersistRecordingToWav).
  bool Save(const std::string &path, uint16_t channels) {
    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    FileHandle file = fs->Open(path.c_str(), "w");
    if (!file) return false;
    auto fail = [&] {
      file.reset();
      fs->DeleteFile(path.c_str());
      return false;
    };
    if (!WavHeaderWriter::WriteHeader(file.get(), kCaptureSampleRate, channels, 16))
      return fail();
    const auto *bytes = reinterpret_cast<const uint8_t *>(samples_.data());
    const uint32_t total = static_cast<uint32_t>(samples_.size() * sizeof(int16_t));
    uint32_t written = 0;
    while (written < total) {
      if (Cancelled()) return fail();
      const uint32_t chunk = std::min(kSaveChunkBytes, total - written);
      if (file->Write(bytes + written, chunk, 1) != static_cast<int>(chunk))
        return fail();
      written += chunk;
      SetPercent(static_cast<uint8_t>((static_cast<uint64_t>(written) * 100U) / total));
    }
    if (!file->Sync()) return fail();
    const uint32_t frames = total / (channels * sizeof(int16_t));
    if (!WavHeaderWriter::UpdateFileSize(file.get(), frames, channels, 2))
      return fail();
    return true;
  }

  std::mutex m_;
  std::condition_variable cv_;
  std::thread worker_;
  std::unique_ptr<HostAudioCapture> capture_;
  Phase phase_ = Phase::Idle;
  RecordSource source_ = LineIn;
  bool monoLineIn_ = false;
  int32_t lineGainDb_ = 0;
  int32_t micGainDb_ = 0;
  std::string path_;
  uint16_t channels_ = 2;
  int thresholdLevel_ = 0;
  bool triggered_ = false;
  uint64_t maxFrames_ = 0;
  std::vector<int16_t> samples_;
  bool stopRequested_ = false;
  bool cancelRequested_ = false;
  bool sessionPending_ = false;
  bool savedAudio_ = false;
  uint8_t percent_ = 0;
};

Recorder &Rec() {
  static Recorder recorder;
  return recorder;
}

} // namespace

void HostRecordSetCapture(std::unique_ptr<HostAudioCapture> capture) {
  Rec().SetCapture(std::move(capture));
}
int32_t HostRecordLineInGainDb() { return Rec().LineGain(); }
int32_t HostRecordMicGainDb() { return Rec().MicGain(); }

bool StartRecording(const char *filename, uint8_t threshold, uint32_t milliseconds) {
  return Rec().Start(filename, threshold, milliseconds);
}
void StopRecording() { Rec().Stop(); }
void CancelRecording() { Rec().Cancel(); }
void RequestStopRecording() { Rec().RequestStop(); }
bool WaitForRecordingStop(uint32_t timeoutMs) { return Rec().WaitForStop(timeoutMs); }
void FinishStopRecording() { Rec().Finish(); }
void StartMonitoring() { Rec().StartMonitoring(); }
void StopMonitoring() { Rec().StopMonitoring(); }
void SetInputSource(RecordSource source) { Rec().SetSource(source); }
void SetLineInGain(int32_t gainDb) { Rec().SetLineGain(gainDb); }
void SetMicGain(int32_t gainDb) { Rec().SetMicGain(gainDb); }
bool IsRecordingActive() { return Rec().Active(); }
bool IsSavingRecording() { return Rec().Saving(); }
uint8_t GetSavingProgressPercent() { return Rec().Percent(); }
bool DidLastRecordingCaptureAudio() { return Rec().SavedAudio(); }
