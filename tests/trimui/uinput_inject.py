#!/usr/bin/env python3

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

"""Virtual gamepad for tests/trimui/emulated_smoke.sh (Linux, needs /dev/uinput, root).

Creates a uinput device shaped like the Trimui pad as SDL numbers it: buttons 0-12
are consecutive key codes from BTN_SOUTH, the D-pad is hat 0, the left stick is
axes 0/1. Then reads commands, one per line, from the FIFO given as the argument:

  hat up|down|left|right   press and release a D-pad direction
  button N [N...]          press and release SDL button(s) N together
  chord                    hold Select(6)+Start(7)+L1(4)+R1(5), then release
  quit                     destroy the device and exit

The /dev/input node is created with mknod because a container's /dev is a copy
taken at start and does not see nodes that appear later. Prints "node <path>"
once the device is ready. Python 3.5 compatible (Ubuntu 16.04).
"""
import fcntl
import os
import struct
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BTN_SOUTH = 0x130
ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y = 0x00, 0x01, 0x10, 0x11
BUTTONS = 13

# ioctl numbers for aarch64/x86 (_IOC with the generic layout).
UI_SET_EVBIT = 0x40045564
UI_SET_KEYBIT = 0x40045565
UI_SET_ABSBIT = 0x40045567
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502
UI_GET_SYSNAME = 0x8040552C  # _IOC(read, 'U', 44, 64)

HATS = {"up": (ABS_HAT0Y, -1), "down": (ABS_HAT0Y, 1), "left": (ABS_HAT0X, -1), "right": (ABS_HAT0X, 1)}
CHORD = (6, 7, 4, 5)


def emit(fd, etype, code, value):
    # struct input_event: timeval (2 longs), u16 type, u16 code, s32 value.
    os.write(fd, struct.pack("llHHi", 0, 0, etype, code, value))


def sync(fd):
    emit(fd, EV_SYN, 0, 0)


def create_device():
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    for ev in (EV_KEY, EV_ABS, EV_SYN):
        fcntl.ioctl(fd, UI_SET_EVBIT, ev)
    for index in range(BUTTONS):
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_SOUTH + index)
    for axis in (ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y):
        fcntl.ioctl(fd, UI_SET_ABSBIT, axis)
    # struct uinput_user_dev: name[80], input_id (4 x u16), ff_effects_max,
    # then absmax/absmin/absfuzz/absflat[64] (s32 each).
    absmax, absmin = [0] * 64, [0] * 64
    for axis, lo, hi in ((ABS_X, -32768, 32767), (ABS_Y, -32768, 32767),
                         (ABS_HAT0X, -1, 1), (ABS_HAT0Y, -1, 1)):
        absmin[axis], absmax[axis] = lo, hi
    name = b"TRIMUI Player1 (emulated)".ljust(80, b"\0")
    setup = struct.pack("80sHHHHI", name, 0x03, 0x1234, 0x5678, 1, 0)
    setup += struct.pack("64i", *absmax) + struct.pack("64i", *absmin) + bytes(2 * 64 * 4)
    os.write(fd, setup)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    return fd


def make_node(fd):
    buf = fcntl.ioctl(fd, UI_GET_SYSNAME, bytes(64))
    sysname = buf.split(b"\0")[0].decode()
    input_dir = "/sys/devices/virtual/input/" + sysname
    deadline = time.time() + 5
    while time.time() < deadline:
        for entry in sorted(os.listdir(input_dir)) if os.path.isdir(input_dir) else []:
            if entry.startswith("event"):
                major, minor = open(os.path.join(input_dir, entry, "dev")).read().strip().split(":")
                os.makedirs("/dev/input", exist_ok=True)
                path = "/dev/input/" + entry
                if not os.path.exists(path):
                    os.mknod(path, 0o666 | 0o020000, os.makedev(int(major), int(minor)))
                return path
        time.sleep(0.05)
    raise SystemExit("uinput_inject: no event node under " + input_dir)


def tap(fd, keys, hold=0.15):
    for key in keys:
        emit(fd, EV_KEY, BTN_SOUTH + key, 1)
        sync(fd)
    time.sleep(hold)
    for key in reversed(keys):
        emit(fd, EV_KEY, BTN_SOUTH + key, 0)
        sync(fd)
    time.sleep(hold)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: uinput_inject.py FIFO")
    fifo = sys.argv[1]
    fd = create_device()
    print("node", make_node(fd), flush=True)
    while True:
        # Re-open per round: the shell writes a few lines then closes.
        with open(fifo) as commands:
            for line in commands:
                words = line.split()
                if not words:
                    continue
                if words[0] == "hat" and len(words) == 2 and words[1] in HATS:
                    axis, value = HATS[words[1]]
                    emit(fd, EV_ABS, axis, value)
                    sync(fd)
                    time.sleep(0.15)
                    emit(fd, EV_ABS, axis, 0)
                    sync(fd)
                    time.sleep(0.15)
                elif words[0] == "button" and len(words) >= 2:
                    tap(fd, [int(word) for word in words[1:]])
                elif words[0] == "chord":
                    tap(fd, list(CHORD), hold=0.3)
                elif words[0] == "quit":
                    fcntl.ioctl(fd, UI_DEV_DESTROY)
                    return
                else:
                    print("uinput_inject: bad command: " + line.strip(), file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
