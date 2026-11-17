#!/usr/bin/env python3

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

"""Host side of tests/trimui/display_check.sh: turns a raw /dev/fb0 page into a BMP and checks it.

usage: python3 -I display_check.py RAW OUT_BMP --width W --height H --stride BYTES [--smoke SMOKE_BMP]
                                   [--panel-w 1280] [--content 720]

RAW is the visible page of the framebuffer (height rows of stride bytes, 32 bpp, B G R X). The
checks print "ok:" or "FAIL:" and the exit status is the number of failures (capped at 1):
  - the picture is not black (enough lit pixels, some contrast);
  - the content region (content x height, centred) holds the picture and the borders left and right
    of it are black;
  - against the --smoke screenshot (the same project screen rendered by the app itself): a coarse
    block-average comparison of the content region, a perceptual tolerance and not a byte match.
"""
import argparse
import struct
import sys


def read_bmp(path):
    """Returns (w, h, rows) with rows top-down lists of (r, g, b) bytes per pixel, from a 24/32-bit BMP."""
    data = open(path, "rb").read()
    off = struct.unpack_from("<I", data, 10)[0]
    w, h, _planes, bpp = struct.unpack_from("<iiHH", data, 18)
    if bpp not in (24, 32):
        raise SystemExit("smoke bmp: %d bpp unsupported" % bpp)
    flip = h > 0
    h = abs(h)
    bpr = ((w * bpp // 8) + 3) // 4 * 4
    px = bpp // 8
    rows = []
    for y in range(h):
        base = off + y * bpr
        rows.append([(data[base + x * px + 2], data[base + x * px + 1], data[base + x * px]) for x in range(w)])
    if flip:
        rows.reverse()
    return w, h, rows


def write_bmp(path, w, h, rows):
    bpr = (w * 3 + 3) // 4 * 4
    out = bytearray(b"BM" + struct.pack("<IHHI", 54 + bpr * h, 0, 0, 54))
    out += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, bpr * h, 2835, 2835, 0, 0)
    pad = b"\0" * (bpr - w * 3)
    for row in reversed(rows):
        for r, g, b in row:
            out += bytes((b, g, r))
        out += pad
    open(path, "wb").write(out)


def luma(p):
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) // 1000


def block_means(rows, x0, x1, blocks):
    """Mean luma of a blocks x blocks grid over columns x0..x1 of all rows."""
    h = len(rows)
    bw = (x1 - x0) / blocks
    bh = h / blocks
    grid = []
    for by in range(blocks):
        line = []
        for bx in range(blocks):
            ys = range(int(by * bh), int((by + 1) * bh))
            xs = range(x0 + int(bx * bw), x0 + int((bx + 1) * bw))
            total = n = 0
            for y in ys[::2]:
                row = rows[y]
                for x in xs[::2]:
                    total += luma(row[x])
                    n += 1
            line.append(total / max(n, 1))
        grid.append(line)
    return grid


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("out_bmp")
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    ap.add_argument("--stride", type=int, required=True)
    ap.add_argument("--smoke")
    ap.add_argument("--content", type=int, default=720)
    a = ap.parse_args()

    raw = open(a.raw, "rb").read()
    need = a.stride * a.height
    failures = 0

    def ok(msg):
        print("ok:", msg)

    def fail(msg):
        nonlocal failures
        failures += 1
        print("FAIL:", msg)

    if len(raw) < need:
        fail("capture is %d bytes, a %dx%d page at stride %d needs %d" % (len(raw), a.width, a.height, a.stride, need))
        return 1
    rows = []
    for y in range(a.height):
        base = y * a.stride
        rows.append([(raw[base + x * 4 + 2], raw[base + x * 4 + 1], raw[base + x * 4]) for x in range(a.width)])
    write_bmp(a.out_bmp, a.width, a.height, rows)
    print("capture: %dx%d, stride %d, written to %s" % (a.width, a.height, a.stride, a.out_bmp))

    left = (a.width - a.content) // 2
    right = left + a.content
    lit = sum(1 for row in rows for p in row[::4] if luma(p) > 24)
    total = sum(len(row[::4]) for row in rows)
    lumas = [luma(p) for row in rows for p in row[::8]]
    spread = max(lumas) - min(lumas)
    if lit * 100 >= total * 2 and spread >= 64:
        ok("picture is not black (%.1f%% of sampled pixels lit, luma spread %d)" % (100.0 * lit / total, spread))
    else:
        fail("picture is black or flat (%.1f%% lit, luma spread %d)" % (100.0 * lit / total, spread))

    inner_lit = sum(1 for row in rows for p in row[left:right:4] if luma(p) > 24)
    inner_total = sum(len(row[left:right:4]) for row in rows)
    border_lit = sum(1 for row in rows for p in (row[0:left:4] + row[right::4]) if luma(p) > 24)
    border_total = sum(len(row[0:left:4]) + len(row[right::4]) for row in rows)
    if inner_lit * 100 >= inner_total * 5:
        ok("content region x %d..%d holds the picture (%.1f%% lit)" % (left, right, 100.0 * inner_lit / inner_total))
    else:
        fail("content region x %d..%d is empty (%.1f%% lit)" % (left, right, 100.0 * inner_lit / inner_total))
    if border_total and border_lit * 1000 <= border_total * 5:
        ok("borders (%d px each side) are black (%.2f%% lit)" % (left, 100.0 * border_lit / border_total))
    else:
        fail("borders are not black (%.2f%% lit): the content is not centred at %d px or has been scaled" % (
            100.0 * border_lit / max(border_total, 1), a.content))

    if a.smoke:
        sw, sh, srows = read_bmp(a.smoke)
        blocks = 24
        cap = block_means(rows, left, right, blocks)
        sm = block_means(srows, 0, sw, blocks)
        diffs = [abs(cap[y][x] - sm[y][x]) for y in range(blocks) for x in range(blocks)]
        mean_diff = sum(diffs) / len(diffs)
        big = sum(1 for d in diffs if d > 48)
        print("smoke compare: smoke %dx%d vs capture content region, mean block luma diff %.1f, %d/%d blocks differ by > 48" % (
            sw, sh, mean_diff, big, len(diffs)))
        if mean_diff <= 24 and big * 10 <= len(diffs):
            ok("matches the --smoke screenshot within tolerance")
        else:
            fail("does not match the --smoke screenshot (mean diff %.1f, %d blocks far off)" % (mean_diff, big))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
