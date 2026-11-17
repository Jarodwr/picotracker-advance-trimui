// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostFileSystem.h"

#include <cassert>
#include <filesystem>
#include <random>
#include <string>

namespace fs = std::filesystem;

int main() {
  const auto nonce = std::random_device{}();
  const fs::path base = fs::temp_directory_path() /
                        ("picotracker-hostfs-" + std::to_string(nonce));
  assert(!fs::exists(base));
  fs::create_directory(base);
  struct Cleanup {
    fs::path path;
    ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); }
  } cleanup{base};

  HostFileSystem host(base / "data");
  assert(host.valid());
  assert(host.makeDir("/projects"));
  assert(host.makeDir("/projects/DEMO/samples", true));
  assert(!host.makeDir("/../../escape", true));

  auto file = host.Open("/projects/DEMO/song.dat", "wb");
  assert(file);
  assert(file->Write("track", 1, 5) == 5);
  assert(file->Sync());
  file.reset();

  file = host.Open("/projects/DEMO/song.dat", "rb");
  char bytes[6]{};
  assert(file && file->Read(bytes, 5) == 5);
  assert(std::string(bytes) == "track");
  file.reset();

  assert(host.CopyFile("/projects/DEMO/song.dat", "/projects/DEMO/copy.dat"));
  assert(host.MoveFile("/projects/DEMO/copy.dat", "/projects/DEMO/renamed.dat"));
  assert(host.exists("/projects/DEMO/renamed.dat"));

  etl::vector<FileSystemEntry, 8> entries;
  assert(host.ListDirectory("/projects/DEMO", &entries, ".DAT", false));
  assert(entries.size() == 3); // samples directory plus two matching files
  assert(entries[0].name == "renamed.dat" || entries[0].name == "samples");
  etl::vector<FileSystemEntry, 1> smallEntries;
  assert(!host.ListDirectory("/projects/DEMO", &smallEntries, ".dat", false));
  assert(host.GetLastListDirectoryStatus() == DirectoryListStatus::Truncated);
  assert(host.DeleteFile("/projects/DEMO/renamed.dat"));
  assert(!host.DeleteDir("/projects/DEMO")); // nonempty

  fs::create_symlink(base / "outside.dat", host.root() / "escape.dat");
  assert(!host.Open("/escape.dat", "rb"));
  assert(!host.Open("/../outside.dat", "wb"));
  assert(!host.CopyFile("/projects/DEMO/song.dat", "/escape.dat"));
  assert(!host.ListDirectory("/../../", &entries, "", false));
  assert(host.GetLastListDirectoryStatus() == DirectoryListStatus::Error);
}
