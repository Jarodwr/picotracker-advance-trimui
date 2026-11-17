// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "Adapters/adv/usb/usb_utils.h"

extern "C" UsbHostControlSurfaceType usbHostControlSurfaceType() {
  return USB_HOST_CONTROL_SURFACE_NONE;
}

extern "C" bool usbHostModeActive() { return false; }
