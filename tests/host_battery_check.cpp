// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostBattery.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace {

fs::path root;

void Put(const std::string &rel, const std::string &value) {
  fs::create_directories((root / rel).parent_path());
  std::ofstream(root / rel) << value << "\n";
}

void Fake(const std::string &capacity, const std::string &status,
          const std::string &usb_online) {
  Put("axp2202-battery/capacity", capacity);
  Put("axp2202-battery/status", status);
  Put("axp2202-battery/voltage_now", "4092000");
  Put("axp2202-battery/temp", "264");
  Put("axp2202-usb/online", usb_online);
}

void MissingIsError() {
  HostBattery b((root / "nowhere").string());
  CHECK(b.Read().error);
}

void MapsFields() {
  Fake("73", "Discharging", "0");
  HostBatteryReading r = HostBattery(root.string()).Read();
  CHECK(!r.error && r.percentage == 73 && r.voltage_mv == 4092 &&
        r.temperature_c == 26 && !r.charging);
}

void Charging() {
  Fake("40", "Charging", "1");
  CHECK(HostBattery(root.string()).Read().charging);
  Fake("40", "Discharging", "1"); // usb online, as NextUI reads it
  CHECK(HostBattery(root.string()).Read().charging);
  Fake("100", "Full", "1");
  CHECK(!HostBattery(root.string()).Read().charging);
}

void ClampsAndRejectsGarbage() {
  Fake("250", "Discharging", "0");
  CHECK(HostBattery(root.string()).Read().percentage == 100);
  Fake("abc", "Discharging", "0");
  CHECK(HostBattery(root.string()).Read().error);
}

void CachesForInterval() {
  Fake("80", "Discharging", "0");
  HostBattery b(root.string());
  CHECK(b.Get(1000).percentage == 80);
  Fake("20", "Discharging", "0");
  CHECK(b.Get(1000 + HostBattery::kCacheMs - 1).percentage == 80);
  CHECK(b.Get(1000 + HostBattery::kCacheMs).percentage == 20);
}

} // namespace

int main() {
  root = fs::temp_directory_path() /
         ("picotracker-battery-" + std::to_string(std::random_device{}()));
  fs::create_directories(root);
  MissingIsError();
  MapsFields();
  Charging();
  ClampsAndRejectsGarbage();
  CachesForInterval();
  std::error_code ignored;
  fs::remove_all(root, ignored);
  std::puts("host_battery_check ok");
  return 0;
}
