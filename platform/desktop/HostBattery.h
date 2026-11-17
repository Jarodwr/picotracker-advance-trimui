/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <utility>
#include <string>

// Battery reading in the shape the firmware's BatteryState expects.
struct HostBatteryReading {
  uint8_t percentage = 0;
  uint16_t voltage_mv = 0;
  int16_t temperature_c = 0;
  bool charging = false;
  bool error = true; // no battery information available
};

// Reads the Trimui AXP2202 power supply under a sysfs power_supply directory
// (default /sys/class/power_supply). Missing files give error = true, so
// desktop builds keep reporting "no battery". Results are cached for
// kCacheMs so callers on the UI thread never touch the filesystem more than
// once per interval.
class HostBattery {
public:
  static constexpr uint32_t kCacheMs = 5000;

  explicit HostBattery(std::string root = DefaultRoot())
      : root_(std::move(root)) {}

  // Uncached read.
  HostBatteryReading Read() const;
  // Cached read; now_ms is a monotonic millisecond clock.
  HostBatteryReading Get(uint32_t now_ms);

  // PICOTRACKER_POWER_SUPPLY_ROOT overrides /sys/class/power_supply.
  static std::string DefaultRoot();

private:
  std::string root_;
  HostBatteryReading cached_;
  uint32_t cached_at_ = 0;
  bool have_cache_ = false;
};
