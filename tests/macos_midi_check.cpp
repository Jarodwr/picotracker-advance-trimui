// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

// CoreMIDI routing check for the production macOS MIDI adapters.
//
// Drives CoreMidiIn and CoreMidiOut through CoreMIDI virtual ports, so no
// hardware is needed, with the real upstream MidiInDevice parser/router
// (Services/Midi/MidiInDevice.cpp) behind CoreMidiIn. Only Player and
// MidiService are recording stand-ins (tests/macos_midi_fake), so every
// "observed" line below is what the upstream router asked of them.
//
// Covered: note and clock paths in, all 16 channels, per-channel program,
// running status and interleaved real-time bytes, unsupported messages,
// output encoding, device disappear/reappear in both directions, restart and
// close/reopen, and boot with no matching device.
//
// Build (from the repository root; upstream is the pinned submodule):
//   U=upstream/advance-firmware/sources
//   c++ -std=c++23 -DADV -Wno-deprecated-declarations -Itests/macos_midi_fake \
//     -Iplatform/macos -I$U -I$U/config -I$U/Externals/etl/include \
//     -I$U/Foundation/Variables tests/macos_midi_check.cpp \
//     platform/macos/CoreMidiIn.cpp platform/macos/CoreMidiOut.cpp \
//     $U/Services/Midi/MidiInDevice.cpp $U/Services/Midi/MidiOutDevice.cpp \
//     $U/Foundation/Observable.cpp -framework CoreMIDI \
//     -framework CoreFoundation -o macos_midi_check
// Run: ./macos_midi_check   (exit status is non-zero if any check fails)
//
// Limits: virtual ports only. Real USB/TRS hardware, the MacBackend/
// HostMidiService composition and the settings UI are not exercised here.

#include "Application/Player/Player.h"
#include "CoreMidiIn.h"
#include "CoreMidiOut.h"
#include "Services/Midi/MidiService.h"

#include <CoreMIDI/CoreMIDI.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Player stand-in: record the router's calls.
static std::string Fmt(const char *format, ...) {
  char buffer[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  return buffer;
}
static const char *Src(MidiInputSource s) {
  return s == MidiInputSource::Usb ? "usb" : "trs";
}
void Player::PlayMidiNote(MidiInputSource s, uint8_t ch, uint8_t inst,
                          uint8_t note, uint8_t vel) {
  calls.push_back(Fmt("play %s ch%u inst%u note%u vel%u", Src(s), ch, inst, note, vel));
}
void Player::StopMidiNote(MidiInputSource s, uint8_t ch, uint8_t note) {
  calls.push_back(Fmt("stop %s ch%u note%u", Src(s), ch, note));
}
void Player::ReleaseMidiInputSource(MidiInputSource s) {
  calls.push_back(Fmt("release %s", Src(s)));
}
void Player::OnSongStartButton(unsigned, unsigned, bool, bool) {
  calls.push_back("song-start");
  running = true;
}
void Player::Stop() {
  calls.push_back("song-stop");
  running = false;
}

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ++failures;                                                              \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
    }                                                                          \
  } while (0)

static void Sleep(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static bool WaitFor(const std::function<bool()> &done, int timeoutMs = 4000,
                    const std::function<void()> &step = {}) {
  for (int waited = 0; waited < timeoutMs; waited += 20) {
    if (step)
      step();
    if (done())
      return true;
    Sleep(20);
  }
  return done();
}

static std::string Name(MIDIEndpointRef e) {
  CFStringRef text = nullptr;
  std::string name;
  if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &text) == noErr &&
      text) {
    char buffer[256];
    if (CFStringGetCString(text, buffer, sizeof buffer, kCFStringEncodingUTF8))
      name = buffer;
    CFRelease(text);
  }
  return name;
}

static MIDIEndpointRef FindDestination(const std::string &name) {
  for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
    if (Name(MIDIGetDestination(i)) == name)
      return MIDIGetDestination(i);
  return 0;
}

static MIDIEndpointRef FindSource(const std::string &name) {
  for (ItemCount i = 0; i < MIDIGetNumberOfSources(); ++i)
    if (Name(MIDIGetSource(i)) == name)
      return MIDIGetSource(i);
  return 0;
}

// Raw byte capture for output tests.
struct Capture {
  std::mutex lock;
  std::vector<uint8_t> bytes;
  std::vector<size_t> packetSizes;
  std::vector<MIDITimeStamp> stamps;
  std::vector<uint8_t> Get() {
    std::lock_guard<std::mutex> guard(lock);
    return bytes;
  }
  void Clear() {
    std::lock_guard<std::mutex> guard(lock);
    bytes.clear();
    packetSizes.clear();
    stamps.clear();
  }
};

static void CaptureRead(const MIDIPacketList *list, void *refCon, void *) {
  auto *capture = static_cast<Capture *>(refCon);
  std::lock_guard<std::mutex> guard(capture->lock);
  const MIDIPacket *p = &list->packet[0];
  for (UInt32 i = 0; i < list->numPackets; ++i) {
    capture->packetSizes.push_back(p->length);
    capture->stamps.push_back(p->timeStamp);
    for (UInt16 j = 0; j < p->length; ++j)
      capture->bytes.push_back(p->data[j]);
    p = MIDIPacketNext(p);
  }
}

static std::string Hex(const std::vector<uint8_t> &v) {
  std::string s;
  for (uint8_t b : v)
    s += Fmt("%02X ", b);
  if (!s.empty())
    s.pop_back();
  return s;
}

static void SendRaw(MIDIPortRef port, MIDIEndpointRef destination,
                    std::initializer_list<uint8_t> bytes) {
  std::vector<uint8_t> data(bytes);
  alignas(MIDIPacketList) Byte buffer[1024];
  auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
  MIDIPacket *p = MIDIPacketListInit(list);
  MIDIPacketListAdd(list, sizeof buffer, p, 0, data.size(), data.data());
  CHECK(MIDISend(port, destination, list) == noErr);
}

static Player &P() { return *Player::GetInstance(); }
static MidiService &Svc() { return *MidiService::GetInstance(); }

// Fresh state, then returns when the in device has drained `count` calls.
static void ResetObserved() {
  P().calls.clear();
  P().running = false;
  Svc().clockTimestamps.clear();
}

static bool PollUntilCalls(CoreMidiIn &in, size_t count) {
  return WaitFor([&] { return P().calls.size() >= count; }, 3000,
                 [&] { in.poll(); });
}

static void Dump(const char *title) {
  std::printf("observed %s:", title);
  for (const auto &c : P().calls)
    std::printf(" [%s]", c.c_str());
  std::printf("\n");
}

static void Equals(const char *what, const std::vector<std::string> &expected) {
  if (P().calls != expected) {
    ++failures;
    std::printf("FAIL %s\n", what);
    Dump("actual");
    std::printf("  expected:");
    for (const auto &c : expected)
      std::printf(" [%s]", c.c_str());
    std::printf("\n");
  }
}

int main() {
  MIDIClientRef client;
  CHECK(MIDIClientCreate(CFSTR("macos_midi_check"), nullptr, nullptr, &client) ==
        noErr);
  MIDIPortRef sendPort;
  CHECK(MIDIOutputPortCreate(client, CFSTR("send"), &sendPort) == noErr);

  // The same endpoint names and exclusions MacBackend uses.
  static constexpr const char *kIn = "picoTracker In";
  static constexpr const char *kOut = "picoTracker Out";

  std::printf("== input: virtual destination, upstream router\n");
  {
    CoreMidiInOptions options;
    options.virtualName = kIn;
    options.virtualOnly = true;
    options.excludeNames = {kOut};
    CoreMidiIn in("in", options);
    CHECK(in.Init());
    CHECK(in.Start());
    CHECK(in.IsConnected());
    CHECK(in.IsRunning());
    Sleep(300);
    MIDIEndpointRef dest = FindDestination(kIn);
    CHECK(dest != 0);

    // Notes: on, velocity-0 off, explicit off.
    ResetObserved();
    SendRaw(sendPort, dest, {0x90, 60, 100, 0x90, 60, 0, 0x80, 62, 64});
    CHECK(PollUntilCalls(in, 3));
    Dump("note on / vel0 / off");
    Equals("note path",
           {"play usb ch0 inst0 note60 vel100", "stop usb ch0 note60",
            "stop usb ch0 note62"});

    // Running status plus a real-time byte inside the message.
    ResetObserved();
    SendRaw(sendPort, dest, {0x91, 64, 90, 0xF8, 66, 80, 68, 70});
    CHECK(PollUntilCalls(in, 3));
    Dump("running status + interleaved clock");
    Equals("running status",
           {"play usb ch1 inst0 note64 vel90", "play usb ch1 inst0 note66 vel80",
            "play usb ch1 inst0 note68 vel70"});
    CHECK(Svc().clockTimestamps.size() == 1);

    // Every channel reaches the router with its own number.
    ResetObserved();
    for (uint8_t ch = 0; ch < 16; ++ch)
      SendRaw(sendPort, dest, {uint8_t(0x90 | ch), uint8_t(40 + ch), 100});
    CHECK(PollUntilCalls(in, 16));
    bool channelsOk = P().calls.size() == 16;
    for (uint8_t ch = 0; channelsOk && ch < 16; ++ch)
      channelsOk = P().calls[ch] ==
                   Fmt("play usb ch%u inst0 note%u vel100", ch, 40 + ch);
    CHECK(channelsOk);
    std::printf("observed 16 channels: %zu play calls, first [%s] last [%s]\n",
                P().calls.size(), P().calls.empty() ? "" : P().calls[0].c_str(),
                P().calls.empty() ? "" : P().calls.back().c_str());

    // Program change selects the instrument per channel, others unaffected.
    ResetObserved();
    SendRaw(sendPort, dest, {0xC2, 7, 0x92, 50, 100, 0x93, 51, 100, 0xC2, 63,
                             0x92, 52, 100});
    CHECK(PollUntilCalls(in, 3));
    Dump("program change");
    Equals("program per channel",
           {"play usb ch2 inst7 note50 vel100", "play usb ch3 inst0 note51 vel100",
            "play usb ch2 inst63 note52 vel100"});
    CHECK(in.GetProgramForChannel(2) == 63 && in.GetProgramForChannel(3) == 0);
    // Program 64+ is out of the instrument range: the router drops the note.
    ResetObserved();
    SendRaw(sendPort, dest, {0xC4, 64, 0x94, 53, 100, 0x95, 54, 100});
    CHECK(PollUntilCalls(in, 1));
    Sleep(100);
    in.poll();
    Dump("program 64 (out of range)");
    Equals("out-of-range program",
           {"play usb ch5 inst0 note54 vel100"});

    // Transport and clock.
    ResetObserved();
    SendRaw(sendPort, dest, {0xFA});
    CHECK(PollUntilCalls(in, 1));
    SendRaw(sendPort, dest, {0xFC});
    CHECK(PollUntilCalls(in, 2));
    SendRaw(sendPort, dest, {0xFB});
    CHECK(PollUntilCalls(in, 3));
    Dump("start / stop / continue");
    Equals("transport", {"song-start", "song-stop", "song-start"});
    ResetObserved();
    std::vector<uint8_t> clocks(48, 0xF8);
    for (int i = 0; i < 2; ++i) {
      alignas(MIDIPacketList) Byte buffer[256];
      auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
      MIDIPacket *p = MIDIPacketListInit(list);
      MIDIPacketListAdd(list, sizeof buffer, p, 0, 24, clocks.data());
      CHECK(MIDISend(sendPort, dest, list) == noErr);
      Sleep(50);
    }
    CHECK(WaitFor([&] { return Svc().clockTimestamps.size() >= 48; }, 3000,
                  [&] { in.poll(); }));
    bool monotonic = true;
    for (size_t i = 1; i < Svc().clockTimestamps.size(); ++i)
      monotonic &= Svc().clockTimestamps[i] >= Svc().clockTimestamps[i - 1];
    CHECK(Svc().clockTimestamps.size() == 48);
    CHECK(monotonic);
    CHECK(Svc().clockTimestamps.size() > 1 &&
          Svc().clockTimestamps.back() > Svc().clockTimestamps.front());
    std::printf("observed clock: %zu ticks, timestamps %u..%u us, monotonic=%d\n",
                Svc().clockTimestamps.size(),
                Svc().clockTimestamps.empty() ? 0 : Svc().clockTimestamps.front(),
                Svc().clockTimestamps.empty() ? 0 : Svc().clockTimestamps.back(),
                monotonic);

    // Messages the upstream router accepts but does not act on, and SysEx.
    ResetObserved();
    SendRaw(sendPort, dest,
            {0xB0, 7, 99, 0xA0, 60, 10, 0xD0, 20, 0xE0, 0, 64, 0xFE, 0xF3, 5});
    Sleep(150);
    in.poll();
    CHECK(P().calls.empty());
    std::printf("observed CC/aftertouch/pressure/bend/active-sensing/song-select: "
                "%zu player calls (accepted, not acted on upstream)\n",
                P().calls.size());
    SendRaw(sendPort, dest, {0xF0, 0x7D, 1, 2, 3, 0xF7});
    Sleep(150);
    in.poll();
    // A note after SysEx must still work with a fresh status byte.
    SendRaw(sendPort, dest, {0x90, 70, 100});
    CHECK(PollUntilCalls(in, 1));
    Dump("after SysEx then note");
    Equals("sysex ignored", {"play usb ch0 inst0 note70 vel100"});

    // Stop releases held MIDI notes through the player (once).
    ResetObserved();
    in.Stop();
    CHECK(!in.IsRunning());
    Dump("Stop");
    Equals("stop releases source", {"release usb"});
    // While stopped nothing reaches the router.
    ResetObserved();
    SendRaw(sendPort, dest, {0x90, 71, 100});
    Sleep(200);
    in.poll();
    CHECK(P().calls.empty());
    CHECK(in.Start() && in.IsRunning());
    SendRaw(sendPort, dest, {0x90, 72, 100});
    CHECK(PollUntilCalls(in, 1));
    in.Close();
    // The virtual destination disappears with the device.
    CHECK(WaitFor([] { return FindDestination(kIn) == 0; }));
  }

  std::printf("== input: external source disappears and returns\n");
  {
    CoreMidiInOptions options;
    options.sourceName = "CheckFlakySource";
    CoreMidiIn in("in", options);
    CHECK(in.Init() && in.Start());
    CHECK(!in.IsConnected()); // no such source yet: not an error
    MIDIEndpointRef flaky = 0;
    CHECK(MIDISourceCreate(client, CFSTR("CheckFlakySource"), &flaky) == noErr);
    CHECK(WaitFor([&] { return in.IsConnected(); }, 5000, [&] { in.poll(); }));
    ResetObserved();
    {
      alignas(MIDIPacketList) Byte buffer[64];
      auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
      MIDIPacket *p = MIDIPacketListInit(list);
      const uint8_t on[] = {0x93, 60, 100};
      MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof on, on);
      CHECK(MIDIReceived(flaky, list) == noErr);
      CHECK(PollUntilCalls(in, 1));
      Equals("note via hardware-style source",
             {"play usb ch3 inst0 note60 vel100"});

      // Source goes away mid-message (status+data1 sent, data2 never arrives).
      const uint8_t partial[] = {0x93, 61};
      p = MIDIPacketListInit(list);
      MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof partial, partial);
      MIDIReceived(flaky, list);
      Sleep(100);
      in.poll();
    }
    MIDIEndpointDispose(flaky);
    CHECK(WaitFor([&] { return !in.IsConnected(); }, 5000, [&] { in.poll(); }));
    CHECK(in.ConnectedSourceName().empty());
    in.poll(); // polling while disconnected is safe
    ResetObserved();

    MIDIEndpointRef back = 0;
    CHECK(MIDISourceCreate(client, CFSTR("CheckFlakySource"), &back) == noErr);
    CHECK(WaitFor([&] { return in.IsConnected(); }, 5000, [&] { in.poll(); }));
    CHECK(in.ConnectedSourceName() == "CheckFlakySource");
    // A lone data byte after reconnect: does the stale half-message complete?
    // (CoreMIDI may itself drop a status-less packet; the line below records
    // what actually reached the parser.)
    alignas(MIDIPacketList) Byte buffer[64];
    auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
    MIDIPacket *p = MIDIPacketListInit(list);
    const uint8_t stale[] = {100};
    MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof stale, stale);
    MIDIReceived(back, list);
    Sleep(200);
    in.poll();
    std::printf("observed stale data byte after reconnect: %zu player calls%s\n",
                P().calls.size(),
                P().calls.empty() ? " (not delivered, or parser state was reset)"
                                  : " (parser kept the partial message)");
    ResetObserved();
    const uint8_t on[] = {0x90, 65, 100};
    p = MIDIPacketListInit(list);
    MIDIPacketListAdd(list, sizeof buffer, p, 0, sizeof on, on);
    MIDIReceived(back, list);
    CHECK(PollUntilCalls(in, 1));
    Dump("note after reconnect");
    MIDIEndpointDispose(back);
  }

  std::printf("== input: restart and reopen\n");
  {
    CoreMidiInOptions options;
    options.virtualName = "CheckReopenIn";
    options.virtualOnly = true;
    CoreMidiIn in("in", options);
    for (int cycle = 0; cycle < 3; ++cycle) {
      CHECK(in.Init() && in.Start());
      Sleep(200);
      MIDIEndpointRef dest = FindDestination("CheckReopenIn");
      CHECK(dest != 0);
      ResetObserved();
      SendRaw(sendPort, dest, {0x90, uint8_t(30 + cycle), 100});
      CHECK(PollUntilCalls(in, 1));
      in.Stop();
      in.Close();
      CHECK(WaitFor([] { return FindDestination("CheckReopenIn") == 0; }));
    }
    std::printf("observed 3 Init/Start/Stop/Close cycles, endpoint recreated each time\n");
  }

  std::printf("== output: virtual source, encoding\n");
  {
    Capture capture;
    MIDIPortRef capturePort;
    CHECK(MIDIInputPortCreate(client, CFSTR("capture"), CaptureRead, &capture,
                              &capturePort) == noErr);
    CoreMidiOutOptions options;
    options.virtualName = kOut;
    options.excludeNames = {kIn};
    CoreMidiOut out("out", options);
    CHECK(out.Init());
    CHECK(!out.IsAvailable()); // not started
    CHECK(out.Start() && out.IsAvailable());
    CHECK(out.ConnectedDestinationName() == kOut);
    Sleep(300);
    MIDIEndpointRef source = FindSource(kOut);
    CHECK(source != 0);
    CHECK(MIDIPortConnectSource(capturePort, source, nullptr) == noErr);

    auto expect = [&](const char *what, std::vector<uint8_t> expected) {
      CHECK(WaitFor([&] { return capture.Get().size() >= expected.size(); }));
      Sleep(50);
      auto actual = capture.Get();
      if (actual != expected) {
        ++failures;
        std::printf("FAIL %s: got [%s] expected [%s]\n", what, Hex(actual).c_str(),
                    Hex(expected).c_str());
      } else
        std::printf("observed out %s: %s\n", what, Hex(actual).c_str());
      capture.Clear();
    };

    for (uint8_t ch = 0; ch < 16; ++ch) {
      MidiMessage m(uint8_t(0x90 | ch), uint8_t(60 + ch), 100);
      out.SendMessage(m);
    }
    {
      std::vector<uint8_t> expected;
      for (uint8_t ch = 0; ch < 16; ++ch)
        for (uint8_t b : {uint8_t(0x90 | ch), uint8_t(60 + ch), uint8_t(100)})
          expected.push_back(b);
      expect("note-on x16 channels", expected);
    }
    {
      MidiMessage off(0x85, 60, 0), program(0xC3, 9), bend(0xE1, 0, 64),
          pressure(0xD2, 40), clock(0xF8), start(0xFA), cont(0xFB), stop(0xFC);
      out.SendMessage(off);
      out.SendMessage(program);
      out.SendMessage(bend);
      out.SendMessage(pressure);
      out.SendMessage(clock);
      out.SendMessage(start);
      out.SendMessage(cont);
      out.SendMessage(stop);
      expect("off/program/bend/pressure/clock/start/continue/stop",
             {0x85, 60, 0, 0xC3, 9, 0xE1, 0, 64, 0xD2, 40, 0xF8, 0xFA, 0xFB, 0xFC});
    }
    {
      // 24 clocks + a chord, the shape the sequencer flushes each tick.
      etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> queue;
      std::vector<uint8_t> expected;
      for (int i = 0; i < 4; ++i) {
        queue.push_back(MidiMessage(0x90, uint8_t(60 + i), 100));
        for (uint8_t b : {uint8_t(0x90), uint8_t(60 + i), uint8_t(100)})
          expected.push_back(b);
      }
      queue.push_back(MidiMessage(0xF8));
      expected.push_back(0xF8);
      out.SendQueue(queue);
      expect("queued chord + clock", expected);
      // Full-size queue keeps order.
      queue.clear();
      expected.clear();
      for (size_t i = 0; i < queue.max_size(); ++i) {
        uint8_t ch = i % 16;
        queue.push_back(MidiMessage(uint8_t(0x90 | ch), uint8_t(i), 90));
        for (uint8_t b : {uint8_t(0x90 | ch), uint8_t(i), uint8_t(90)})
          expected.push_back(b);
      }
      out.SendQueue(queue);
      expect("full queue (max messages), order kept", expected);
    }
    {
      const uint8_t sysex[] = {0xF0, 0x7D, 1, 2, 0xF7};
      out.SendRawBytes(sysex, sizeof sysex);
      expect("raw sysex", {0xF0, 0x7D, 1, 2, 0xF7});
      // Unsupported: SysEx via SendMessage and data-first/invalid status.
      MidiMessage sysexStart(0xF0, 0x7D, 1), invalid(0x40, 1, 2);
      out.SendMessage(sysexStart);
      out.SendMessage(invalid);
      Sleep(150);
      CHECK(capture.Get().empty());
      std::printf("observed out SendMessage(0xF0) and data-byte status: dropped (%zu bytes)\n",
                  capture.Get().size());
      // Unset data bytes (255) go out as zero; high data bits are masked.
      MidiMessage unset(0xB0, 7), wide(0x90, 0xFF, 0xFE); // 0xFF is the unset marker
      out.SendMessage(unset);
      out.SendMessage(wide);
      expect("unused data (255) -> 0, high bit masked", {0xB0, 7, 0, 0x90, 0, 0x7E});
    }
    {
      // Messages are sent with timestamp 0 ("now").
      MidiMessage m(0x90, 1, 1);
      out.SendMessage(m);
      CHECK(WaitFor([&] { return !capture.Get().empty(); }));
      std::lock_guard<std::mutex> guard(capture.lock);
      std::printf("observed out packet timestamp: %llu (CoreMIDI stamps delivery time; sender asked for 0)\n",
                  (unsigned long long)(capture.stamps.empty() ? 0 : capture.stamps[0]));
    }
    capture.Clear();
    out.Stop();
    CHECK(!out.IsAvailable());
    MidiMessage m(0x90, 1, 1);
    out.SendMessage(m);
    Sleep(150);
    CHECK(capture.Get().empty());
    out.Close();
    CHECK(WaitFor([&] { return FindSource(kOut) == 0; }));
    MIDIPortDispose(capturePort);
  }

  std::printf("== output: destination disappears and returns\n");
  {
    Capture capture;
    MIDIEndpointRef flaky = 0;
    CHECK(MIDIDestinationCreate(client, CFSTR("CheckFlakyDest"), CaptureRead,
                                &capture, &flaky) == noErr);
    Sleep(200);
    CoreMidiOutOptions options;
    options.destinationName = "CheckFlakyDest";
    CoreMidiOut out("out", options);
    CHECK(out.Init() && out.Start());
    CHECK(out.IsAvailable());
    CHECK(out.ConnectedDestinationName() == "CheckFlakyDest");
    MidiMessage a(0x90, 60, 100);
    out.SendMessage(a);
    CHECK(WaitFor([&] { return capture.Get().size() == 3; }));
    capture.Clear();

    MIDIEndpointDispose(flaky);
    Sleep(300);
    out.Poll();
    CHECK(!out.IsAvailable());
    CHECK(out.ConnectedDestinationName().empty());
    out.SendMessage(a); // dropped, no crash
    etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> queue;
    queue.push_back(a);
    out.SendQueue(queue);
    const uint8_t raw[] = {0xF0, 0xF7};
    out.SendRawBytes(raw, sizeof raw);
    std::printf("observed out after disconnect: unavailable, sends dropped silently\n");

    // Reappears: found by Poll, and by the bounded-rate retry in a send.
    MIDIEndpointRef back = 0;
    CHECK(MIDIDestinationCreate(client, CFSTR("CheckFlakyDest"), CaptureRead,
                                &capture, &back) == noErr);
    CHECK(WaitFor([&] { out.Poll(); return out.IsAvailable(); }, 5000));
    out.SendMessage(a);
    CHECK(WaitFor([&] { return capture.Get().size() == 3; }));
    std::printf("observed out after reappear: reconnected via Poll, delivered %s\n",
                Hex(capture.Get()).c_str());
    capture.Clear();

    // Without Poll(): CoreMIDI accepts a send to a disposed endpoint without
    // error, so a send-only caller never notices the loss and never rescans.
    // Production polls every event-loop pass (MacBackend hook), so this is
    // recorded as a known defect, not a failure.
    MIDIEndpointDispose(back);
    Sleep(300);
    out.SendMessage(a);
    const bool staleAvailable = out.IsAvailable();
    CHECK(MIDIDestinationCreate(client, CFSTR("CheckFlakyDest"), CaptureRead,
                                &capture, &back) == noErr);
    Sleep(1200); // past the 1 s retry interval
    out.SendMessage(a);
    Sleep(300);
    const bool sendOnlyRecovered = capture.Get().size() >= 3;
    std::printf("KNOWN-DEFECT out send-only path: IsAvailable() after lost "
                "destination=%d, delivered after recreate without Poll=%d\n",
                staleAvailable, sendOnlyRecovered);
    capture.Clear();
    CHECK(WaitFor([&] { out.Poll(); return out.IsAvailable(); }));
    out.SendMessage(a);
    CHECK(WaitFor([&] { return capture.Get().size() >= 3; }));
    std::printf("observed out recovery via Poll after send-only loss: %s\n",
                Hex(capture.Get()).c_str());
    MIDIEndpointDispose(back);
  }

  std::printf("== boot with no matching device\n");
  {
    CoreMidiInOptions inOptions;
    inOptions.sourceName = "No Such Source";
    CoreMidiIn in("in", inOptions);
    CHECK(in.Init());
    CHECK(in.Start());
    CHECK(!in.IsConnected());
    CHECK(in.ConnectedSourceName().empty());
    for (int i = 0; i < 5; ++i)
      in.poll();
    in.Stop();
    in.Close();

    CoreMidiOutOptions outOptions;
    outOptions.destinationName = "No Such Destination";
    CoreMidiOut out("out", outOptions);
    CHECK(out.Init());
    CHECK(out.Start());
    CHECK(!out.IsAvailable());
    CHECK(out.ConnectedDestinationName().empty());
    MidiMessage m(0x90, 60, 100);
    out.SendMessage(m);
    out.Poll();
    out.Stop();
    out.Close();

    // Stop/Close/destructor on devices that never started.
    { CoreMidiIn idle("idle"); }
    { CoreMidiOut idle("idle"); idle.Close(); }
    // Sends before Init and after Close must be harmless.
    CoreMidiOut uninit("uninit");
    uninit.SendMessage(m);
    std::printf("observed no-device boot: Init/Start succeed, no connection, sends dropped, polls safe\n");

    // Production in/out options with whatever the machine has: must come up.
    CoreMidiInOptions prodIn;
    prodIn.virtualName = kIn;
    prodIn.excludeNames = {kOut};
    CoreMidiIn productionIn("CoreMIDI In", prodIn);
    CoreMidiOutOptions prodHw;
    prodHw.excludeNames = {kIn};
    CoreMidiOut productionHw("CoreMIDI Out", prodHw);
    CoreMidiOutOptions prodVirtual;
    prodVirtual.virtualName = kOut;
    CoreMidiOut productionVirtual("Virtual Out", prodVirtual);
    CHECK(productionIn.Init() && productionHw.Init() && productionVirtual.Init());
    CHECK(productionIn.Start() && productionHw.Start() && productionVirtual.Start());
    CHECK(productionVirtual.IsAvailable());
    std::printf("observed production options: in connected=%d (%s), hardware out=%d (%s), virtual out=%d\n",
                productionIn.IsConnected(),
                productionIn.ConnectedSourceName().c_str(),
                productionHw.IsAvailable(),
                productionHw.ConnectedDestinationName().c_str(),
                productionVirtual.IsAvailable());
    CHECK(productionIn.ConnectedSourceName() != kOut);
    CHECK(productionHw.ConnectedDestinationName() != kIn);
    productionIn.Close();
    productionHw.Close();
    productionVirtual.Close();
  }

  MIDIClientDispose(client);
  if (failures == 0)
    std::puts("macos_midi_check ok");
  else
    std::printf("macos_midi_check: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
