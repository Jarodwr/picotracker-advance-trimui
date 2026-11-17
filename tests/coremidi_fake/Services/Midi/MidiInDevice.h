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

// Minimal stand-in for the upstream MidiInDevice so the CoreMIDI input adapter
// can be exercised without linking the Player and Song graph. It mirrors the
// protected surface the adapter uses; the real header is used in the app build.
#include "Services/Midi/MidiInputSource.h"

#include <cstdint>
#include <vector>

class MidiInDevice {
public:
  explicit MidiInDevice(const char *) {}
  virtual ~MidiInDevice() {}
  bool Init() { return initDriver(); }
  void Close() { closeDriver(); }
  virtual bool Start() = 0;
  virtual void Stop() = 0;
  virtual void poll() = 0;

  struct Received {
    uint8_t byte;
    uint32_t timestampUs;
  };
  std::vector<Received> received;

protected:
  virtual bool initDriver() = 0;
  virtual void closeDriver() = 0;
  virtual bool startDriver() = 0;
  virtual void stopDriver() = 0;
  virtual MidiInputSource getInputSource() const = 0;
  void processMidiData(uint8_t data, uint32_t timestampUs = 0) {
    received.push_back({data, timestampUs});
  }
  bool isRunning_ = false;
};

// Upstream defines the pure virtuals so adapters can chain to them.
inline bool MidiInDevice::Start() {
  isRunning_ = true;
  return startDriver();
}
inline void MidiInDevice::Stop() {
  isRunning_ = false;
  stopDriver();
}
