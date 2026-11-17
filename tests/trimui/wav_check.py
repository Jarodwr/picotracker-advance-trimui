#!/usr/bin/env python3

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

"""Checks a WAV recorded by PicoTracker (tests/trimui/record_check.sh).

usage: wav_check.py FILE EXPECTED_SECONDS [--silent-ok] [--any-length]

Prints one "key: value" line per fact and exits 0 when the file loads as 16-bit PCM at 44100 Hz,
its length is within the slack of EXPECTED_SECONDS, it holds signal (non-zero RMS, unless
--silent-ok for a source known to be silent; --any-length skips the length check) and it follows the mono policy
(owner decision for the Smart Pro, single built-in mic on the left channel): the file is mono, or
stereo with the right channel a copy of the left. Exits 1 on the first failed check, listing all.
"""
import math
import struct
import sys
import wave

SLACK_BELOW = 1.5  # the injected presses take about a second to arrive on each side
SLACK_ABOVE = 3.0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    silent_ok = "--silent-ok" in sys.argv
    any_length = "--any-length" in sys.argv
    if len(args) != 2:
        print(__doc__)
        return 2
    path, expected = args[0], float(args[1])
    failures = []
    try:
        w = wave.open(path, "rb")
    except (OSError, EOFError, wave.Error) as e:
        print("loads: no (%s)" % e)
        return 1
    channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    data = w.readframes(frames)
    print("loads: yes")
    print("channels: %d" % channels)
    print("sample_rate: %d" % rate)
    print("bits: %d" % (width * 8))
    seconds = frames / rate if rate else 0.0
    print("frames: %d" % frames)
    print("seconds: %.2f" % seconds)
    if width != 2:
        failures.append("not 16-bit")
        samples = []
    else:
        samples = struct.unpack("<%dh" % (len(data) // 2), data)
    if rate != 44100:
        failures.append("sample rate %d, expected 44100" % rate)
    if not any_length and not (expected - SLACK_BELOW <= seconds <= expected + SLACK_ABOVE):
        failures.append("length %.2f s, expected about %.1f s" % (seconds, expected))
    chans = [samples[i::channels] for i in range(channels)] if channels else []

    def rms(a):
        return math.sqrt(sum(x * x for x in a) / len(a)) if a else 0.0

    for index, chan in enumerate(chans):
        print("rms_ch%d: %.1f" % (index, rms(chan)))
        print("peak_ch%d: %d" % (index, max((abs(x) for x in chan), default=0)))
    if not chans or rms(chans[0]) == 0.0:
        if not silent_ok:
            failures.append("left channel is silent (RMS 0)")
    if channels == 2:
        differs = sum(1 for a, b in zip(chans[0], chans[1]) if a != b)
        print("right_equals_left: %s" % ("yes" if differs == 0 else "no (%d of %d frames differ)" % (differs, frames)))
        if differs:
            failures.append("stereo file whose right channel is not a copy of the left (owner decision: mono, L copied to R)")
    elif channels != 1:
        failures.append("%d channels" % channels)
    for f in failures:
        print("FAIL: " + f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
