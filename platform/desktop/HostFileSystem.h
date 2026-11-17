/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "System/FileSystem/FileSystem.h"

#include <filesystem>
#include <optional>

class HostFileSystem final : public FileSystem {
public:
  explicit HostFileSystem(const std::filesystem::path &dataDirectory);

  bool valid() const { return valid_; }
  const std::filesystem::path &root() const { return root_; }

  FileHandle Open(const char *name, const char *mode) override;
  bool ListDirectory(const char *absoluteDir,
                     etl::ivector<FileSystemEntry> *entries,
                     const char *filter, bool directoriesOnly,
                     bool includeHidden = false) override;
  bool DeleteFile(const char *name) override;
  bool DeleteDir(const char *name) override;
  bool exists(const char *path) override;
  bool makeDir(const char *path, bool parents = false) override;
  bool CopyFile(const char *source, const char *destination) override;
  bool MoveFile(const char *source, const char *destination) override;
  bool isExFat() override { return false; }

private:
  std::optional<std::filesystem::path> Resolve(const char *firmwarePath) const;
  std::filesystem::path root_;
  bool valid_ = false;
};
