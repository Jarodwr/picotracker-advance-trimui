// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostVolume.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

std::string HostVolumeFile::DefaultPath() {
  const char *env = std::getenv("PICOTRACKER_VOLUME_FILE");
  return env && *env ? env : "/tmp/shm/SharedSettings";
}

int HostVolumeFile::Read() {
  std::FILE *file = std::fopen(path_.c_str(), "rb");
  if (!file) return -1;
  int32_t ints[9];
  const size_t got = std::fread(ints, 1, sizeof(ints), file);
  std::fclose(file);
  if (got != sizeof(ints)) return -1;
  const int32_t step = ints[8] ? ints[3] : ints[4];
  if (step < 0 || step > kMaxStep) return -1;
  return step * 100 / kMaxStep;
}

bool HostVolumeWatcher::Poll(uint32_t nowMs) {
  if (polled_ && nowMs - lastPollMs_ < kPollMs) return false;
  polled_ = true;
  lastPollMs_ = nowMs;
  const int volume = source_.Read();
  if (volume < 0) return false;
  const bool first = last_ < 0;
  const bool changed = !first && volume != last_;
  last_ = volume;
  return changed;
}
