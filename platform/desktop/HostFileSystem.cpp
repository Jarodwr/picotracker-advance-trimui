// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostFileSystem.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

class HostFile final : public I_File {
public:
  explicit HostFile(std::FILE *file) : file_(file) {}
  ~HostFile() override { Close(); }
  int Read(void *ptr, int size) override {
    if (!file_ || !ptr || size < 0) return -1;
    const auto count = std::fread(ptr, 1, static_cast<size_t>(size), file_);
    return std::ferror(file_) ? -1 : static_cast<int>(count);
  }
  int GetC() override { return file_ ? std::fgetc(file_) : EOF; }
  int Write(const void *ptr, int size, int nmemb) override {
    if (!file_ || !ptr || size < 0 || nmemb < 0) return -1;
    const auto bytes = static_cast<size_t>(size) * static_cast<size_t>(nmemb);
    if (size && bytes / static_cast<size_t>(size) != static_cast<size_t>(nmemb))
      return -1;
    const auto written = std::fwrite(ptr, 1, bytes, file_);
    return std::ferror(file_) ? -1 : static_cast<int>(written);
  }
  void Seek(long offset, int whence) override {
    if (file_) std::fseek(file_, offset, whence);
  }
  long Tell() override { return file_ ? std::ftell(file_) : -1; }
  int Error() override { return file_ ? std::ferror(file_) : 1; }
  bool Sync() override { return file_ && std::fflush(file_) == 0; }
  void Dispose() override { delete this; }

private:
  bool Close() override {
    if (!file_) return true;
    const bool ok = std::fclose(file_) == 0;
    file_ = nullptr;
    return ok;
  }
  std::FILE *file_;
};

bool Within(const fs::path &root, const fs::path &candidate) {
  auto parent = root.begin();
  auto child = candidate.begin();
  for (; parent != root.end(); ++parent, ++child) {
    if (child == candidate.end() || *parent != *child) return false;
  }
  return true;
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

} // namespace

HostFileSystem::HostFileSystem(const fs::path &dataDirectory) {
  std::error_code error;
  fs::create_directories(dataDirectory, error);
  if (error) return;
  root_ = fs::canonical(dataDirectory, error);
  valid_ = !error && fs::is_directory(root_);
}

std::optional<fs::path> HostFileSystem::Resolve(const char *firmwarePath) const {
  if (!valid_ || !firmwarePath || firmwarePath[0] != '/') return std::nullopt;
  // Reject host-native alternate separators and drive syntax as well as parent
  // components, so firmware paths have the same meaning on all hosts.
  if (std::strchr(firmwarePath, '\\') || std::strchr(firmwarePath, ':'))
    return std::nullopt;
  fs::path relative;
  fs::path cursor = root_;
  for (const auto &part : fs::path(firmwarePath).relative_path()) {
    if (part == "..") return std::nullopt;
    if (part == "." || part.empty()) continue;
    relative /= part;
    cursor /= part;
    std::error_code linkError;
    const auto status = fs::symlink_status(cursor, linkError);
    if (fs::is_symlink(status) ||
        (linkError && linkError != std::errc::no_such_file_or_directory))
      return std::nullopt;
  }
  std::error_code error;
  const auto resolved = fs::weakly_canonical(root_ / relative, error);
  if (error || !Within(root_, resolved)) return std::nullopt;
  return resolved;
}

FileHandle HostFileSystem::Open(const char *name, const char *mode) {
  const auto path = Resolve(name);
  if (!path || !mode || !*mode || std::strspn(mode, "rwa+b") != std::strlen(mode))
    return {};
  if (*mode != 'r' && *mode != 'w' && *mode != 'a') return {};
  std::FILE *file = std::fopen(path->string().c_str(), mode);
  return file ? MakeFileHandle(new HostFile(file)) : FileHandle{};
}

bool HostFileSystem::ListDirectory(const char *absoluteDir,
                                   etl::ivector<FileSystemEntry> *entries,
                                   const char *filter, bool directoriesOnly,
                                   bool includeHidden) {
  SetLastListDirectoryStatus(DirectoryListStatus::Ok);
  if (!entries) {
    SetLastListDirectoryStatus(DirectoryListStatus::Error);
    return false;
  }
  entries->clear();
  const auto path = Resolve(absoluteDir);
  std::error_code error;
  if (!path || !fs::is_directory(*path, error) || error) {
    SetLastListDirectoryStatus(DirectoryListStatus::Error);
    return false;
  }
  std::vector<fs::directory_entry> items;
  for (fs::directory_iterator it(*path, error), end; !error && it != end;
       it.increment(error)) items.push_back(*it);
  if (error) {
    SetLastListDirectoryStatus(DirectoryListStatus::Error);
    return false;
  }
  std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
    return a.path().filename() < b.path().filename();
  });
  const std::string wanted = Lower(filter ? filter : "");
  for (const auto &item : items) {
    const auto name = item.path().filename().string();
    const bool hidden = !name.empty() && name.front() == '.';
    if (hidden && !includeHidden) continue;
    const bool directory = item.is_directory(error);
    if (error) break;
    if (directoriesOnly && !directory) continue;
    if (!directory && Lower(name).find(wanted) == std::string::npos) continue;
    // Do not offer symlinks leading outside the data directory.
    if (!Resolve((fs::path(absoluteDir) / name).string().c_str())) continue;
    if (name.size() >= PFILENAME_SIZE) {
      SetLastListDirectoryStatus(DirectoryListStatus::Error);
      return false;
    }
    if (entries->full()) {
      SetLastListDirectoryStatus(DirectoryListStatus::Truncated);
      return false;
    }
    FileSystemEntry result;
    result.name = name.c_str();
    result.type = directory ? PFT_DIR : PFT_FILE;
    result.size = directory ? 0 : item.file_size(error);
    if (error) break;
    result.isHidden = hidden;
    entries->push_back(result);
  }
  if (error) SetLastListDirectoryStatus(DirectoryListStatus::Error);
  return !error;
}

bool HostFileSystem::DeleteFile(const char *name) {
  const auto path = Resolve(name);
  std::error_code error;
  return path && fs::is_regular_file(*path, error) && !error &&
         fs::remove(*path, error) && !error;
}

bool HostFileSystem::DeleteDir(const char *name) {
  const auto path = Resolve(name);
  std::error_code error;
  return path && *path != root_ && fs::is_directory(*path, error) && !error &&
         fs::remove(*path, error) && !error;
}

bool HostFileSystem::exists(const char *path) {
  const auto resolved = Resolve(path);
  std::error_code error;
  return resolved && fs::exists(*resolved, error) && !error;
}

bool HostFileSystem::makeDir(const char *path, bool parents) {
  const auto resolved = Resolve(path);
  if (!resolved) return false;
  std::error_code error;
  if (fs::exists(*resolved, error)) return !error && fs::is_directory(*resolved);
  if (error) return false;
  const bool created = parents ? fs::create_directories(*resolved, error)
                               : fs::create_directory(*resolved, error);
  return created && !error;
}

bool HostFileSystem::CopyFile(const char *source, const char *destination) {
  const auto from = Resolve(source), to = Resolve(destination);
  std::error_code error;
  return from && to && fs::is_regular_file(*from, error) && !error &&
         fs::copy_file(*from, *to, fs::copy_options::overwrite_existing, error) &&
         !error;
}

bool HostFileSystem::MoveFile(const char *source, const char *destination) {
  const auto from = Resolve(source), to = Resolve(destination);
  if (!from || !to || *from == root_ || *to == root_) return false;
  std::error_code error;
  fs::rename(*from, *to, error);
  return !error;
}
