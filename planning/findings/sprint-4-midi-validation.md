# Sprint 4 MIDI validation (macOS)

Card C-5. Check: `tests/macos_midi_check.cpp` (build line in its header; it
uses recording stand-ins in `tests/macos_midi_fake/`). It runs the production
`CoreMidiIn` and `CoreMidiOut` over CoreMIDI **virtual ports only** (no
hardware), with the **real upstream** `MidiInDevice.cpp` parser/router behind
`CoreMidiIn`. Only `Player` and `MidiService` are stand-ins that record what
the router asks of them (note start/stop, transport, clock). Production
sources were not touched.

## Environment

- macOS 26.5.2, arm64, no MIDI hardware attached (the machine has no external
  CoreMIDI sources or destinations other than the check's own virtual ones).
- Endpoint names and exclusions are the ones `MacBackend` uses
  (`picoTracker In`, `picoTracker Out`).
- Three consecutive full runs: 0 failures each.

## Results (0 failures)

| Area | Observed |
| --- | --- |
| Input notes | `90 60 100`, velocity-0 note-on and `80` reach the router as play / stop / stop, source `usb`, channel 0, instrument 0 |
| Running status | `91 64 90 F8 66 80 68 70` gives three notes on channel 1 with one clock tick; the real-time byte does not break the message |
| Channel routing in | channels 0-15 each arrive with their own channel number and note |
| Program change | per-channel instrument: `C2 7` then channel 2 plays instrument 7, channel 3 stays 0, `C2 63` retargets channel 2 |
| Transport and clock | `FA`/`FC`/`FB` give song-start / song-stop / song-start; 48 clocks (two 24-byte packets) give 48 `OnMidiClock` calls with non-decreasing microsecond timestamps spanning about 55 ms |
| Input Stop/Start | `Stop` releases held notes once (`release usb`); bytes sent while stopped are discarded; restart works |
| Reopen | three Init/Start/Stop/Close cycles recreate and remove the virtual destination each time |
| External source loss | a named source can appear after `Start`, is connected by `poll()`, disappears (`IsConnected()` false), and reconnects on return; notes flow again |
| Output encoding | 16 channels of note-on, note-off, program (2 bytes), bend (3), channel pressure (2), clock/start/continue/stop (1 each), a queue of the maximum size in order, raw SysEx, all byte-exact on a raw input port |
| Output loss | destination disposed: `Poll()` makes it unavailable, `SendMessage`/`SendQueue`/`SendRawBytes` are silently dropped; on return `Poll()` reconnects and delivery resumes |
| No-device boot | named source/destination that does not exist: `Init`/`Start` succeed, nothing connected, polls and sends are safe; devices that never started, or sends before `Init` and after `Close`, are harmless; the production in/out/virtual options come up (virtual out available, hardware in/out empty) |

## Unsupported or ignored paths (recorded, not defects in the adapters)

- **Upstream router ignores** control change, polyphonic aftertouch, channel
  pressure, pitch bend, active sensing and song select: accepted and parsed,
  no player call (upstream has TODOs for these). Only notes, program change,
  clock and start/stop/continue do anything.
- **Input SysEx is not handled upstream.** The router tolerates it without
  crashing and a following status byte restores normal parsing, but SysEx
  content is discarded.
- **Program numbers 64-127 select nothing:** the router drops the note
  (instrument range is 0-63). `Player` is not told.
- **Input source is always `usb`** (ADR 0013); nothing reports `trs`.
- **Output cannot send SysEx through `SendMessage`** (`F0` and an invalid
  status are dropped); SysEx needs `SendRawBytes`, which works.
- **Output unset data bytes (255) go out as 0**, and data bytes are masked to
  7 bits (`0xFE` goes out as `7E`); message timestamps are always requested as
  0 ("now"), so there is no scheduled/jitter-free timing.

## Defects found

1. **D-1, low: output send path never notices a lost destination.** After the
   destination endpoint is disposed, `MIDISend` still returns success, so
   `Transmit` never clears the cached destination. `IsAvailable()` stays true
   and the 1 s send-driven retry in `Ready()` never runs, so a new endpoint
   with the same name is not found until `Poll()` runs. Observed: after
   recreating the destination, a send alone delivered nothing; `Poll()`
   restored delivery. `MacBackend` calls `Poll()` every pump through
   `HostMidiService::SetPollHook`, so production recovers within one pass; any
   caller that does not poll gets silent loss. `CoreMidiOut.cpp` comment says
   "sends also retry at a bounded rate while disconnected", which is true only
   for failures CoreMIDI reports. Suggested fix: validate the cached endpoint
   against `MIDIGetDestination` in `Ready()` at the retry interval.
2. **D-2, observation: partial message across reconnect not demonstrated.**
   `CoreMidiIn::Rescan` does not reset the upstream parser when the source
   changes, so a half message from the old source could pair with the first
   data byte of a new one. The check tried this (status + one data byte, source
   disposed, new source sends a bare data byte) and no note was produced;
   CoreMIDI appears to drop a status-less packet before it reaches the
   adapter, so the case was **not verified** either way.

## Not verified

- **Real hardware:** USB/TRS devices, class-compliant keyboards, devices that
  send timing clock continuously or SysEx dumps.
- **Physical unplug:** removal is simulated by disposing virtual endpoints,
  which exercises the same `kMIDIMsg*` and endpoint-list paths but not a
  driver teardown.
- **`MacBackend` / `HostMidiService` composition and the executable:** the
  submodule is not checked out in the card worktree, so the app was not built.
  The adapters were exercised with the same options and names directly, but
  `HostMidiService` slot layout, `Install()` ordering and the settings/UI
  device list were not run. The real upstream `Player` and `MidiService`
  (note allocation, clock-to-tempo, MIDI sync settings) were not linked.
- **Packet-list API timing:** clock jitter was only checked for monotonic
  timestamps over two bursts, not tempo accuracy against a real clock source.
