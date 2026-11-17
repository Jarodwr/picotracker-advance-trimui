// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "System/Console/Trace.h"

#include <cstdarg>
#include <cstdio>

Trace::Trace() : logger_(nullptr) {}

void Trace::trace_uart_putc(int value, void *) { std::fputc(value, stderr); }

void Trace::VLog(const char *category, const char *format, va_list &args) {
  std::fprintf(stderr, "[%s] ", category);
  std::vfprintf(stderr, format, args);
  std::fputc('\n', stderr);
}

void Trace::Log(const char *category, const char *format, ...) {
  va_list args;
  va_start(args, format);
  VLog(category, format, args);
  va_end(args);
}

void Trace::Debug(const char *format, ...) {
#ifndef NDEBUG
  va_list args;
  va_start(args, format);
  VLog("DEBUG", format, args);
  va_end(args);
#else
  (void)format;
#endif
}

void Trace::Error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VLog("ERROR", format, args);
  va_end(args);
}

void Trace::RegisterEtlErrorHandler() {}

void Trace::AddLine(const char *line) {
  if (logger_) logger_->AddLine(line);
}

Trace::Logger *Trace::SetLogger(Trace::Logger &logger) {
  auto *previous = logger_;
  logger_ = &logger;
  return previous;
}
