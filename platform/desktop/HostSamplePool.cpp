// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostSamplePool.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

HostSamplePool::HostSamplePool(uint32_t limitBytes)
    : limitBytes_(std::min<uint32_t>(limitBytes,
                                      std::numeric_limits<int32_t>::max())) {}

void HostSamplePool::Reset() {
  for (uint32_t index = 0; index < count_; ++index) {
    wav_[index].Close();
    wav_[index].SetSampleBuffer(nullptr);
    nameStore_[index][0] = '\0';
  }
  count_ = 0;
  allocations_.clear();
  usedBytes_ = 0;
}

void HostSamplePool::ReclaimOrphans() {
  for (auto it = allocations_.begin(); it != allocations_.end();) {
    const auto *address = reinterpret_cast<short *>(it->data.get());
    bool referenced = false;
    for (uint32_t index = 0; index < count_; ++index) {
      if (wav_[index].GetSampleBuffer(0) == address) {
        referenced = true;
        break;
      }
    }
    if (referenced) ++it;
    else {
      usedBytes_ -= it->bytes;
      it = allocations_.erase(it);
    }
  }
}

bool HostSamplePool::CheckSampleFits(uint32_t sampleSize) {
  ReclaimOrphans();
  return sampleSize <= limitBytes_ - usedBytes_;
}

uint32_t HostSamplePool::GetAvailableSampleStorageSpace() {
  ReclaimOrphans();
  return limitBytes_ - usedBytes_;
}

bool HostSamplePool::loadSample(const char *name) {
  if (!name || count_ >= MAX_SAMPLES) return false;
  ReclaimOrphans();
  const uint32_t slot = count_;
  if (!wav_[slot].Open(name)) return false;
  const uint32_t bytes = wav_[slot].GetDiskSize(-1);
  if (bytes == 0 || !CheckSampleFits(bytes)) {
    wav_[slot].Close();
    return false;
  }
  auto buffer = std::unique_ptr<uint16_t[]>(
      new (std::nothrow) uint16_t[(bytes + 1U) / 2U]);
  if (!buffer) {
    wav_[slot].Close();
    return false;
  }
  wav_[slot].SetSampleBuffer(reinterpret_cast<short *>(buffer.get()));
  wav_[slot].Rewind();
  alignas(uint16_t) std::array<uint8_t, BUFFER_SIZE> scratch{};
  uint32_t written = 0;
  while (written < bytes) {
    uint32_t chunk = 0;
    if (!wav_[slot].Read(scratch.data(), BUFFER_SIZE, &chunk) || chunk == 0 ||
        chunk > bytes - written) {
      wav_[slot].Close();
      wav_[slot].SetSampleBuffer(nullptr);
      return false;
    }
    std::memcpy(reinterpret_cast<uint8_t *>(buffer.get()) + written,
                scratch.data(), chunk);
    written += chunk;
  }
  wav_[slot].Close();
  std::strncpy(nameStore_[slot], name, MAX_INSTRUMENT_FILENAME_LENGTH);
  nameStore_[slot][MAX_INSTRUMENT_FILENAME_LENGTH] = '\0';
  try {
    allocations_.push_back({std::move(buffer), bytes});
  } catch (const std::bad_alloc &) {
    wav_[slot].SetSampleBuffer(nullptr);
    return false;
  }
  usedBytes_ += bytes;
  ++count_;
  return true;
}

bool HostSamplePool::unloadSample(uint32_t index) {
  if (index >= count_) return false;
  const auto *address = wav_[index].GetSampleBuffer(0);
  const auto it = std::find_if(allocations_.begin(), allocations_.end(),
                               [address](const Allocation &allocation) {
    return allocation.data.get() == address;
  });
  if (it == allocations_.end()) return false;
  usedBytes_ -= it->bytes;
  allocations_.erase(it);
  for (uint32_t next = index; next + 1 < count_; ++next) {
    wav_[next] = std::move(wav_[next + 1]);
    std::memcpy(nameStore_[next], nameStore_[next + 1],
                MAX_INSTRUMENT_FILENAME_LENGTH + 1);
  }
  wav_[count_ - 1].Close();
  wav_[count_ - 1].SetSampleBuffer(nullptr);
  nameStore_[count_ - 1][0] = '\0';
  --count_;
  SetChanged();
  SamplePoolEvent event;
  event.type_ = SPET_DELETE;
  event.index_ = static_cast<int>(index);
  NotifyObservers(&event);
  return true;
}
