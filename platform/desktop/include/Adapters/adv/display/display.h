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

#include <cstdint>

// Host compile shim for the firmware's bitmap error screen. The desktop
// application will route this drawing through SDL when it is linked.
void display_draw_mono_bitmap_rgb565(uint16_t x, uint16_t y,
                                     const uint8_t *bitmap, uint16_t width,
                                     uint16_t height, uint16_t row_bytes,
                                     uint16_t fg_color565,
                                     uint16_t bg_color565);
