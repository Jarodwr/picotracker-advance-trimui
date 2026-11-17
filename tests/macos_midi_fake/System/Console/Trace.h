/*
 * Copyright (c) 2018 Discodirt
 * Copyright (c) 2024 xiphonics, inc.
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: BSD-3-Clause AND GPL-3.0-only
 *
 * BSD-3-Clause applies to the declarations mirrored from the upstream picoTracker
 * firmware; GPL-3.0-only applies to the wrapper's own additions. Both apply to
 * the file as a whole.
 */

#pragma once

struct Trace {
  static void Debug(const char *, ...) {}
  static void Log(const char *, const char *, ...) {}
};
