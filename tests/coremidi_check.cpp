// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CoreMidiIn.h"
#include "CoreMidiOut.h"

#include <CoreMIDI/CoreMIDI.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Verifies the CoreMIDI adapters over virtual ports, no hardware needed:
//   raw virtual source -> CoreMidiIn (virtual destination only)
//   CoreMidiOut (virtual source) -> raw input port
static std::atomic<int> outBytes{0};
static uint8_t outData[64];

static void RawRead(const MIDIPacketList *list, void *, void *) {
  const MIDIPacket *p = &list->packet[0];
  for (UInt32 i = 0; i < list->numPackets; ++i) {
    for (UInt16 j = 0; j < p->length; ++j) {
      int n = outBytes.load();
      if (n < 64) {
        outData[n] = p->data[j];
        outBytes.store(n + 1);
      }
    }
    p = MIDIPacketNext(p);
  }
}

static void Sleep(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int main() {
  MIDIClientRef client;
  assert(MIDIClientCreate(CFSTR("check"), nullptr, nullptr, &client) == noErr);

  // Input: send through a raw virtual source the adapter connects to.
  MIDIEndpointRef rawSource;
  assert(MIDISourceCreate(client, CFSTR("CheckSource"), &rawSource) == noErr);
  Sleep(200);
  {
    CoreMidiInOptions options;
    options.sourceName = "CheckSource";
    CoreMidiIn in("in", options);
    assert(in.Init());
    assert(in.Start());
    assert(in.IsConnected());
    assert(in.ConnectedSourceName() == "CheckSource");
    alignas(MIDIPacketList) Byte buffer[256];
    auto *list = (MIDIPacketList *)buffer;
    MIDIPacket *p = MIDIPacketListInit(list);
    const uint8_t on[] = {0x90, 60, 100, 0xF8};
    MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof on, on);
    assert(MIDIReceived(rawSource, list) == noErr);
    Sleep(200);
    in.poll();
    assert(in.received.size() == 4);
    assert(in.received[0].byte == 0x90 && in.received[3].byte == 0xF8);
    assert(in.received[0].timestampUs != 0);
    // After Stop nothing more is delivered.
    in.Stop();
    in.received.clear();
    MIDIReceived(rawSource, list);
    Sleep(100);
    in.poll();
    assert(in.received.empty());
    // Restart reconnects; teardown with the device still running is safe.
    assert(in.Start());
    assert(in.IsConnected());
    in.Close();
  }

  // Hotplug: the source disappears and returns while the device runs.
  {
    CoreMidiInOptions options;
    options.sourceName = "Flaky";
    CoreMidiIn in("in", options);
    assert(in.Init() && in.Start());
    assert(!in.IsConnected());
    MIDIEndpointRef flaky;
    assert(MIDISourceCreate(client, CFSTR("Flaky"), &flaky) == noErr);
    for (int i = 0; i < 40 && !in.IsConnected(); ++i) {
      Sleep(100);
      in.poll();
    }
    assert(in.IsConnected());
    MIDIEndpointDispose(flaky);
    for (int i = 0; i < 40 && in.IsConnected(); ++i) {
      Sleep(100);
      in.poll();
    }
    assert(!in.IsConnected());
  }

  // Input virtual-only: a raw port sends to the adapter's virtual destination.
  {
    CoreMidiInOptions options;
    options.virtualName = "CheckVirtualIn";
    options.virtualOnly = true;
    CoreMidiIn in("in", options);
    assert(in.Init() && in.Start() && in.IsConnected());
    Sleep(200);
    MIDIPortRef port;
    assert(MIDIOutputPortCreate(client, CFSTR("p"), &port) == noErr);
    MIDIEndpointRef dest = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i) {
      CoreMidiOut::ListDestinations(); // exercise listing
      CFStringRef n;
      MIDIObjectGetStringProperty(MIDIGetDestination(i),
                                  kMIDIPropertyDisplayName, &n);
      if (n && CFStringCompare(n, CFSTR("CheckVirtualIn"), 0) ==
                   kCFCompareEqualTo)
        dest = MIDIGetDestination(i);
    }
    assert(dest != 0);
    alignas(MIDIPacketList) Byte buffer[64];
    auto *list = (MIDIPacketList *)buffer;
    MIDIPacket *p = MIDIPacketListInit(list);
    const uint8_t cc[] = {0xB1, 7, 99};
    MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof cc, cc);
    assert(MIDISend(port, dest, list) == noErr);
    Sleep(200);
    in.poll();
    assert(in.received.size() == 3 && in.received[0].byte == 0xB1);
  }

  // Output: virtual source seen by a raw input port.
  {
    MIDIPortRef inPort;
    assert(MIDIInputPortCreate(client, CFSTR("raw"), RawRead, nullptr,
                               &inPort) == noErr);
    CoreMidiOutOptions options;
    options.virtualName = "CheckVirtualOut";
    CoreMidiOut out("out", options);
    assert(out.Init());
    assert(!out.IsAvailable()); // not started yet
    assert(out.Start() && out.IsAvailable());
    Sleep(200);
    MIDIEndpointRef src = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i) {
      CFStringRef n;
      MIDIObjectGetStringProperty(MIDIGetSource(i), kMIDIPropertyDisplayName,
                                  &n);
      if (n && CFStringCompare(n, CFSTR("CheckVirtualOut"), 0) ==
                   kCFCompareEqualTo)
        src = MIDIGetSource(i);
    }
    assert(src != 0);
    assert(MIDIPortConnectSource(inPort, src, nullptr) == noErr);
    MidiMessage note(0x91, 64, 80), program(0xC2, 5), clock(0xF8);
    out.SendMessage(note);
    etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> queue;
    queue.push_back(program);
    queue.push_back(clock);
    out.SendQueue(queue);
    const uint8_t sysex[] = {0xF0, 0x7D, 1, 0xF7};
    out.SendRawBytes(sysex, sizeof sysex);
    Sleep(300);
    const uint8_t expected[] = {0x91, 64, 80, 0xC2, 5, 0xF8,
                                0xF0, 0x7D, 1, 0xF7};
    assert(outBytes.load() == (int)sizeof expected);
    for (size_t i = 0; i < sizeof expected; ++i)
      assert(outData[i] == expected[i]);
    out.Close();
    assert(!out.IsAvailable());
    out.SendMessage(note); // closed device drops silently
  }

  // Output with a destination that does not exist drops and reports unavailable.
  {
    CoreMidiOutOptions options;
    options.destinationName = "Nothing Here";
    CoreMidiOut out("out", options);
    assert(out.Init() && out.Start());
    assert(!out.IsAvailable());
    MidiMessage note(0x90, 60, 100);
    out.SendMessage(note);
  }

  MIDIClientDispose(client);
  std::puts("coremidi_check ok");
  return 0;
}
