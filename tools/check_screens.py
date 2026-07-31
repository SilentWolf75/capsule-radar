#!/usr/bin/env python3
"""Compare simulator screenshots against committed references.

Layout bugs are what this project actually suffers from -- text landing on text, rings
drifting off their image, labels clipped by the round bezel. They were all found by
flashing a board and looking at it, which is slow and easy to skip. The simulator builds
the same ui.cpp and radar_view.cpp against SDL, so the same bugs are visible there, in
CI, before hardware.

    python tools/check_screens.py --shots build/shots            # compare
    python tools/check_screens.py --shots build/shots --update   # accept as new refs

Exit status is non-zero if any screen differs by more than its allowed threshold, so it
can gate a build. Pure stdlib: BMP in, PNG out, no Pillow.
"""
import os
import sys
import zlib
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REFS = os.path.join(ROOT, "tests", "screens")

# Percentage of pixels allowed to differ before a screen is called a regression.
#
# Zero for everything: the mock data is fixed and the simulator parks the sweep and the
# home-marker pulse, so a matching build reproduces every screen exactly. Anything that
# moves on its own is masked below rather than given a tolerance, because a tolerance
# large enough to absorb a clock is large enough to hide a shifted label.
DEFAULT_TOLERANCE = 0.0
TOLERANCE = {}

# (x0, y0, x1, y1) in fractions of the screen, ignored during comparison.
MASKS = {
    # Time + date. Right now the simulator shows the "--:--" placeholder, but only
    # because the clock's tick timer never fires during the capture -- time() on the
    # runner returns a real epoch, so a change to how the shot loop pumps timers would
    # put a live wall clock here. Masked as insurance, not as a description.
    "clock": [(0.20, 0.24, 0.80, 0.50)],
    "about": [(0.30, 0.35, 0.75, 0.39)],   # BUILD line: __DATE__ moves every day
}


def read_bmp(data):
    """Minimal BMP reader: 24/32-bit uncompressed, either row order."""
    if data[:2] != b"BM":
        raise ValueError("not a BMP")
    off = struct.unpack_from("<I", data, 10)[0]
    w = struct.unpack_from("<i", data, 18)[0]
    h = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError("unsupported bpp %d" % bpp)
    top_down = h < 0
    h = abs(h)
    stride = ((w * bpp // 8) + 3) // 4 * 4
    px = bytearray(w * h * 3)
    step = bpp // 8
    for y in range(h):
        src_y = y if top_down else (h - 1 - y)
        src = off + src_y * stride
        dst = y * w * 3
        for x in range(w):
            b, g, r = data[src], data[src + 1], data[src + 2]
            px[dst] = r
            px[dst + 1] = g
            px[dst + 2] = b
            src += step
            dst += 3
    return w, h, bytes(px)


def write_png(path, w, h, rgb):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3:(y + 1) * w * 3]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
           chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def read_png(path):
    """Just enough PNG to read back what write_png produced."""
    d = open(path, "rb").read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    i, idat, w, h = 8, b"", 0, 0
    while i < len(d):
        ln = struct.unpack_from(">I", d, i)[0]
        tag = d[i + 4:i + 8]
        body = d[i + 8:i + 8 + ln]
        if tag == b"IHDR":
            w, h = struct.unpack_from(">II", body, 0)
        elif tag == b"IDAT":
            idat += body
        i += 12 + ln
    raw = zlib.decompress(idat)
    stride = w * 3
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        f = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if f == 1:
            for x in range(3, stride):
                line[x] = (line[x] + line[x - 3]) & 255
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                a = line[x - 3] if x >= 3 else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x - 3] if x >= 3 else 0
                c = prev[x - 3] if x >= 3 else 0
                b = prev[x]
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, bytes(out)


def compare(name, w, h, got, want):
    """Fraction of pixels that differ, ignoring any masked bands."""
    boxes = []
    for fx0, fy0, fx1, fy1 in MASKS.get(name, []):
        boxes.append((int(fx0 * w), int(fy0 * h), int(fx1 * w), int(fy1 * h)))

    differing = 0
    counted = 0
    for y in range(h):
        row = y * w * 3
        for x in range(w):
            if any(x0 <= x < x1 and y0 <= y < y1 for x0, y0, x1, y1 in boxes):
                continue
            counted += 1
            i = row + x * 3
            if got[i] != want[i] or got[i + 1] != want[i + 1] or got[i + 2] != want[i + 2]:
                differing += 1
    return (differing / counted * 100.0) if counted else 0.0


def main():
    args = sys.argv[1:]
    update = "--update" in args
    shots = None
    if "--shots" in args:
        shots = args[args.index("--shots") + 1]
    if not shots or not os.path.isdir(shots):
        sys.exit("usage: check_screens.py --shots <dir with sim BMPs> [--update]")

    os.makedirs(REFS, exist_ok=True)
    bmps = sorted(f for f in os.listdir(shots) if f.endswith(".bmp"))
    if not bmps:
        sys.exit("no .bmp files in %s -- did the simulator run?" % shots)

    failures, updated, checked = [], 0, 0
    for f in bmps:
        # sim writes "<prefix>-<name>.bmp"
        name = f[:-4].split("-", 1)[-1]
        w, h, got = read_bmp(open(os.path.join(shots, f), "rb").read())
        ref = os.path.join(REFS, name + ".png")

        if update or not os.path.exists(ref):
            write_png(ref, w, h, got)
            updated += 1
            print("  %-10s %s  (%dx%d)" % (name, "updated" if update else "NEW reference", w, h))
            continue

        rw, rh, want = read_png(ref)
        if (rw, rh) != (w, h):
            failures.append("%s: size changed %dx%d -> %dx%d" % (name, rw, rh, w, h))
            print("  %-10s SIZE CHANGED %dx%d -> %dx%d" % (name, rw, rh, w, h))
            continue

        pct = compare(name, w, h, got, want)
        tol = TOLERANCE.get(name, DEFAULT_TOLERANCE)
        checked += 1
        if pct > tol:
            out = os.path.join(shots, name + "-actual.png")
            write_png(out, w, h, got)
            failures.append("%s: %.2f%% of pixels differ (allowed %.2f%%)" % (name, pct, tol))
            print("  %-10s DIFFERS %.2f%%   actual written to %s" % (name, pct, out))
        else:
            print("  %-10s ok" % name)

    print()
    if updated:
        print("%d reference(s) written to %s" % (updated, REFS))
    if failures:
        print("%d screen(s) changed:" % len(failures))
        for f in failures:
            print("   -", f)
        print("\nIf the change is intended, re-run with --update and commit the references.")
        return 1
    print("%d screen(s) match their references." % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
