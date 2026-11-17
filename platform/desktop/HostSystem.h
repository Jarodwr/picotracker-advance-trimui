/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "HostBattery.h"
#include "System/System/System.h"
#include <chrono>
#include <random>

class HostSystem final : public System {
public:
  unsigned long GetClock() override { return Millis(); }
  void GetBatteryState(BatteryState &state) override {
    const HostBatteryReading r = battery_.Get(Millis());
    state = {r.percentage, r.voltage_mv, r.temperature_c, r.charging, r.error};
  }
  void SetDisplayBrightness(unsigned char) override {}
  void PostQuitMessage() override { quit_ = true; }
  unsigned int GetMemoryUsage() override { return 0; }
  void PowerDown() override { quit_ = true; }
  void SystemPutChar(int c) override;
  void SystemBootloader() override {}
  void SystemReboot() override { quit_ = true; }
  uint32_t GetRandomNumber() override { return random_(); }
  uint32_t GetDeviceIdentitySeed() override { return 0x50544144u; }
  uint32_t Micros() override;
  uint32_t Millis() override { return Micros() / 1000; }
  bool quit_requested() const { return quit_; }

private:
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
  std::mt19937 random_{std::random_device{}()};
  bool quit_ = false;
  HostBattery battery_;
};
