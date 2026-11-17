/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "System/Timer/Timer.h"

#include <chrono>
#include <memory>
#include <vector>

class HostTimer final : public I_Timer {
public:
  void SetPeriod(float milliseconds) override;
  bool Start() override;
  void Stop() override { running_ = false; }
  float GetPeriod() override { return period_; }
  void Pump(std::chrono::steady_clock::time_point now);

private:
  float period_ = -1.0f;
  bool running_ = false;
  std::chrono::steady_clock::time_point next_;
};

class HostTimerService final : public TimerService {
public:
  I_Timer *CreateTimer() override;
  void TriggerCallback(int milliseconds, timerCallback callback) override;
  void Pump();

private:
  struct Callback {
    std::chrono::steady_clock::time_point due;
    timerCallback function;
  };
  std::vector<std::unique_ptr<HostTimer>> timers_;
  std::vector<Callback> callbacks_;
};
