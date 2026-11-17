/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "HostBattery.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const char kBatteryDir[] = "axp2202-battery";
const char kUsbDir[] = "axp2202-usb";

bool ReadFile(const std::string &path, char *out, size_t size) {
  FILE *f = std::fopen(path.c_str(), "r");
  if (!f) {
    return false;
  }
  size_t n = std::fread(out, 1, size - 1, f);
  std::fclose(f);
  out[n] = 0;
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' ||
                   out[n - 1] == ' ')) {
    out[--n] = 0;
  }
  return true;
}

bool ReadLong(const std::string &path, long &value) {
  char buf[32];
  if (!ReadFile(path, buf, sizeof(buf)) || !*buf) {
    return false;
  }
  char *end = nullptr;
  value = std::strtol(buf, &end, 10);
  return end && *end == 0;
}

} // namespace

std::string HostBattery::DefaultRoot() {
  const char *env = std::getenv("PICOTRACKER_POWER_SUPPLY_ROOT");
  return env && *env ? env : "/sys/class/power_supply";
}

HostBatteryReading HostBattery::Read() const {
  HostBatteryReading r;
  const std::string bat = root_ + "/" + kBatteryDir + "/";
  long capacity = 0;
  if (!ReadLong(bat + "capacity", capacity) || capacity < 0) {
    return r; // error = true
  }
  r.error = false;
  r.percentage = capacity > 100 ? 100 : static_cast<uint8_t>(capacity);

  long v = 0;
  if (ReadLong(bat + "voltage_now", v) && v > 0) { // microvolts
    long mv = v / 1000;
    r.voltage_mv = mv > 65535 ? 65535 : static_cast<uint16_t>(mv);
  }
  if (ReadLong(bat + "temp", v)) { // tenths of a degree Celsius
    r.temperature_c = static_cast<int16_t>(v / 10);
  }

  // NextUI treats usb "online" as charging; also honour the battery status,
  // but a full battery on USB power is not charging.
  char status[32] = "";
  ReadFile(bat + "status", status, sizeof(status));
  long online = 0;
  ReadLong(root_ + "/" + kUsbDir + "/online", online);
  if (std::strcmp(status, "Charging") == 0) {
    r.charging = true;
  } else if (online == 1 && std::strcmp(status, "Full") != 0 &&
             r.percentage < 100) {
    r.charging = true;
  }
  return r;
}

HostBatteryReading HostBattery::Get(uint32_t now_ms) {
  if (!have_cache_ || now_ms - cached_at_ >= kCacheMs) {
    cached_ = Read();
    cached_at_ = now_ms;
    have_cache_ = true;
  }
  return cached_;
}
