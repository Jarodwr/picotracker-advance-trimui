/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Services/Midi/MidiInDevice.h"

#include <memory>
#include <string>
#include <vector>

// CoreMIDI input for the firmware's MidiInDevice contract.
//
// CoreMIDI read callbacks only copy bytes and timestamps into a bounded ring.
// poll() runs on the firmware thread, drains the ring into the upstream
// parser and handles device changes, so no firmware object is touched from a
// CoreMIDI thread. Teardown stops the callbacks before the object goes away.
struct CoreMidiInOptions {
  // Source to read from, matched against the endpoint display name. Empty
  // selects the first available source.
  std::string sourceName;
  // When not empty, also publish a virtual destination with this name so other
  // applications (or tests) can send to the tracker without hardware.
  std::string virtualName;
  // Skip real sources entirely and listen only on the virtual destination.
  bool virtualOnly = false;
  // Endpoint names never selected, e.g. this application's own virtual
  // sources, which would feed its output straight back into its input.
  std::vector<std::string> excludeNames;
};

class CoreMidiIn final : public MidiInDevice {
public:
  explicit CoreMidiIn(const char *name = "CoreMIDI In",
                      CoreMidiInOptions options = {});
  ~CoreMidiIn() override;

  bool Start() override;
  void Stop() override;
  void poll() override;

  // True while a source is connected (or the virtual port is the only input).
  bool IsConnected() const;
  // Display name of the connected source, empty when none.
  std::string ConnectedSourceName() const;
  // Bytes discarded because the ring was full since Init.
  unsigned DroppedBytes() const;

  static std::vector<std::string> ListSources();

protected:
  bool initDriver() override;
  void closeDriver() override;
  bool startDriver() override;
  void stopDriver() override;
  // CoreMIDI endpoints are reported as a single USB-like input, see ADR 0013.
  MidiInputSource getInputSource() const override {
    return MidiInputSource::Usb;
  }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
