// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostSystem.h"
#include "platform.h"

#include <mutex>

class HostMutex final : public SysMutex {
public:
  bool Lock() override { mutex_.lock(); return true; }
  void Unlock() override { mutex_.unlock(); }
private:
  std::mutex mutex_;
};

static HostMutex host_mutex;

uint32_t platform_get_rand() { return System::GetInstance()->GetRandomNumber(); }
void platform_reboot() { System::GetInstance()->SystemReboot(); }
void platform_bootloader() { System::GetInstance()->SystemBootloader(); }
SysMutex *platform_mutex() { return &host_mutex; }
uint32_t millis() { return System::GetInstance()->Millis(); }
uint32_t micros() { return System::GetInstance()->Micros(); }
void platform_brightness(uint8_t value) { System::GetInstance()->SetDisplayBrightness(value); }
int16_t battery_health() { return 0; }
