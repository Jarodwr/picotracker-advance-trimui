// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostTimer.h"

#include <algorithm>
#include <cmath>

using Clock = std::chrono::steady_clock;

void HostTimer::SetPeriod(float milliseconds) {
  period_ = std::isfinite(milliseconds) ? milliseconds : -1.0f;
  if (period_ <= 0.0f) Stop();
  else if (running_) Start();
}

bool HostTimer::Start() {
  if (period_ <= 0.0f) { running_ = false; return false; }
  next_ = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                             std::chrono::duration<float, std::milli>(period_));
  running_ = true;
  return true;
}

void HostTimer::Pump(Clock::time_point now) {
  if (!running_ || now < next_) return;
  SetChanged();
  NotifyObservers();
  if (running_) {
    next_ = now + std::chrono::duration_cast<Clock::duration>(
                      std::chrono::duration<float, std::milli>(period_));
  }
}

I_Timer *HostTimerService::CreateTimer() {
  timers_.push_back(std::make_unique<HostTimer>());
  return timers_.back().get();
}

void HostTimerService::TriggerCallback(int milliseconds, timerCallback callback) {
  if (!callback) return;
  callbacks_.push_back({Clock::now() + std::chrono::milliseconds(std::max(0, milliseconds)), callback});
}

void HostTimerService::Pump() {
  const auto now = Clock::now();
  for (auto &timer : timers_) timer->Pump(now);
  for (size_t index = 0; index < callbacks_.size();) {
    if (callbacks_[index].due > now) { ++index; continue; }
    const auto callback = callbacks_[index].function;
    callbacks_.erase(callbacks_.begin() + index);
    callback();
  }
}
