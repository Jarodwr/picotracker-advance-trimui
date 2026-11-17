/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Services/Midi/MidiOutDevice.h"

#include <memory>
#include <string>
#include <vector>

// CoreMIDI output for the firmware's MidiOutDevice contract.
//
// Messages are sent directly from the calling (firmware) thread. Without a
// destination every send is dropped and IsAvailable() is false, so a saved
// output setting can never reach a missing device.
struct CoreMidiOutOptions {
  // Destination to write to, matched against the endpoint display name. Empty
  // selects the first available destination.
  std::string destinationName;
  // When not empty, publish a virtual source with this name and send through
  // it instead of a hardware destination, so tests need no hardware.
  std::string virtualName;
  // Endpoint names never selected, e.g. this application's own virtual
  // destination, which would feed its output straight back into its input.
  std::vector<std::string> excludeNames;
};

class CoreMidiOut final : public MidiOutDevice {
public:
  explicit CoreMidiOut(const char *name = "CoreMIDI Out",
                       CoreMidiOutOptions options = {});
  ~CoreMidiOut() override;

  bool Init() override;
  void Close() override;
  bool Start() override;
  void Stop() override;

  void SendQueue(etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> &queue) override;
  void SendMessage(MidiMessage &m) override;
  void SendRawBytes(const uint8_t *data, uint8_t len) override;

  // Rescans destinations after a device change. Called by the host event loop;
  // sends also retry at a bounded rate while disconnected.
  void Poll();

  bool IsAvailable() const;
  std::string ConnectedDestinationName() const;

  static std::vector<std::string> ListDestinations();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
