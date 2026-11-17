/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Application/Instruments/SamplePool.h"

#include <cstdint>
#include <memory>
#include <vector>

class HostSamplePool final : public SamplePool {
public:
  static constexpr uint32_t DefaultLimitBytes = 128U * 1024U * 1024U;
  explicit HostSamplePool(uint32_t limitBytes = DefaultLimitBytes);
  void Reset() override;
  bool CheckSampleFits(uint32_t sampleSize) override;
  uint32_t GetAvailableSampleStorageSpace() override;
  bool unloadSample(uint32_t index) override;

protected:
  bool loadSample(const char *name) override;

private:
  struct Allocation {
    std::unique_ptr<uint16_t[]> data;
    uint32_t bytes;
  };
  void ReclaimOrphans();
  uint32_t limitBytes_;
  uint32_t usedBytes_ = 0;
  std::vector<Allocation> allocations_;
};
