// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostVolume.h"
#include "HostVolumeOverlay.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

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

class FakeSource final : public HostVolumeSource {
public:
  int value = -1;
  int reads = 0;
  int Read() override {
    ++reads;
    return value;
  }
};

void WriteSettings(const fs::path &path, int32_t headphones, int32_t speaker,
                   int32_t jack) {
  int32_t ints[33] = {11, 2, 20, headphones, speaker, 0, 0, 0, jack};
  std::FILE *file = std::fopen(path.c_str(), "wb");
  CHECK(file);
  std::fwrite(ints, sizeof(ints[0]), 33, file);
  std::fclose(file);
}

void WatcherChecks() {
  FakeSource source;
  HostVolumeWatcher watcher(source);
  // Unknown readings never count as a baseline.
  CHECK(!watcher.Poll(0));
  CHECK(watcher.Volume() == -1);
  // First valid reading: baseline, no change.
  source.value = 45;
  CHECK(!watcher.Poll(200));
  CHECK(watcher.Volume() == 45);
  // Polled at most every kPollMs.
  const int reads = source.reads;
  source.value = 50;
  CHECK(!watcher.Poll(300));
  CHECK(source.reads == reads);
  CHECK(watcher.Poll(400));
  CHECK(watcher.Volume() == 50);
  // Same value: no change; a failed read keeps the baseline.
  CHECK(!watcher.Poll(600));
  source.value = -1;
  CHECK(!watcher.Poll(800));
  CHECK(watcher.Volume() == 50);
  source.value = 50;
  CHECK(!watcher.Poll(1000));
  source.value = 0;
  CHECK(watcher.Poll(1200));
  CHECK(watcher.Volume() == 0);
}

void OverlayChecks() {
  HostVolumeOverlay overlay;
  CHECK(!overlay.IsVisible());
  CHECK(!overlay.Tick(0));
  overlay.Show(40, 1000);
  CHECK(overlay.IsVisible() && overlay.Volume() == 40);
  CHECK(overlay.Tick(1000)); // appeared: redraw
  CHECK(!overlay.Tick(1500));
  // A second change restarts the timer without another redraw.
  overlay.Show(55, 2000);
  CHECK(!overlay.Tick(2100));
  CHECK(overlay.Volume() == 55);
  CHECK(!overlay.Tick(2000 + HostVolumeOverlay::kShowMs - 1));
  CHECK(overlay.IsVisible());
  CHECK(overlay.Tick(2000 + HostVolumeOverlay::kShowMs));
  CHECK(!overlay.IsVisible());
  CHECK(!overlay.Tick(9000));
  overlay.Show(250, 10000);
  CHECK(overlay.Volume() == 100);

  const int size = 720;
  std::vector<uint32_t> frame(size * size, 0x11223344u);
  overlay.Draw(frame.data(), size);
  CHECK(frame[0] == 0x11223344u); // only paints its own box
  bool painted = false;
  for (uint32_t px : frame) painted |= px != 0x11223344u;
  CHECK(painted);
}

void FileChecks() {
  const fs::path dir = fs::temp_directory_path() / "picotracker_volume_check";
  fs::create_directories(dir);
  const fs::path file = dir / "SharedSettings";
  HostVolumeFile missing((dir / "none").string());
  CHECK(missing.Read() == -1);
  HostVolumeFile source(file.string());
  WriteSettings(file, 4, 9, 0);
  CHECK(source.Read() == 45); // speaker 9/20
  WriteSettings(file, 4, 20, 0);
  CHECK(source.Read() == 100);
  WriteSettings(file, 4, 9, 1);
  CHECK(source.Read() == 20); // headphones when the jack is in
  WriteSettings(file, 4, 21, 0);
  CHECK(source.Read() == -1); // layout we do not understand
  WriteSettings(file, 4, -1, 0);
  CHECK(source.Read() == -1);
  std::FILE *shortFile = std::fopen(file.c_str(), "wb");
  std::fwrite("abc", 1, 3, shortFile);
  std::fclose(shortFile);
  CHECK(source.Read() == -1);
  fs::remove_all(dir);
}

} // namespace

int main() {
  WatcherChecks();
  OverlayChecks();
  FileChecks();
  std::puts("host_volume_check: ok");
  return 0;
}
