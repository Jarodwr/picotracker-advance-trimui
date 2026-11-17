/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// C-63: the system volume as NextUI's keymon.elf/audiomon.elf keep it. The
// app never writes it (ADR 0019); it only learns when it changed so the host
// can draw an indicator. Pure C++ (no SDL).

#include <cstdint>
#include <string>
#include <utility>

class HostVolumeSource {
public:
  virtual ~HostVolumeSource() = default;
  // Volume 0-100, or -1 when unknown (file missing, unexpected content).
  virtual int Read() = 0;
};

// Reads NextUI's shared settings block (a 132-byte mmap'd file of ints,
// /tmp/shm/SharedSettings): headphones volume at int 3, speaker volume at
// int 4, both 0-20, jack-inserted flag at int 8. Anything outside 0-20 or a
// short file reads as unknown, so a layout change fails silent, not wrong.
// PICOTRACKER_VOLUME_FILE overrides the path.
class HostVolumeFile final : public HostVolumeSource {
public:
  static constexpr int kMaxStep = 20;

  explicit HostVolumeFile(std::string path = DefaultPath())
      : path_(std::move(path)) {}
  int Read() override;
  static std::string DefaultPath();

private:
  std::string path_;
};

// Polls a source and reports changes. The first valid reading only sets the
// baseline; unknown readings are ignored (the baseline is kept).
class HostVolumeWatcher {
public:
  static constexpr uint32_t kPollMs = 200;

  explicit HostVolumeWatcher(HostVolumeSource &source) : source_(source) {}

  // Call every loop iteration with a monotonic ms clock. Returns true when the
  // volume changed; the new value is then Volume().
  bool Poll(uint32_t nowMs);
  int Volume() const { return last_; }

private:
  HostVolumeSource &source_;
  int last_ = -1;
  uint32_t lastPollMs_ = 0;
  bool polled_ = false;
};
