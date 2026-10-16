#!/usr/bin/env python3
"""Overlay a geo-project projection onto the matching MAME screenshot and score
the alignment. Local analysis helper for the geometriser pipeline -- it reads
only local capture PNGs (never committed) and the projection PNGs written by
`vf2i960 geo-project`. Pure stdlib (zlib only), no third-party deps.

Usage:
  geo_overlay.py overlay <screenshot.png> <proj.png> <out_overlay.png> [dx dy]
  geo_overlay.py score   <screenshot.png> <proj.png>
  geo_overlay.py offsets <projdir> <screendir>

`overlay` blends the projection over the screenshot (projection colours kept:
red anchor, grey other, dark shadow) and draws its silhouette edges, so the two
can be compared by eye. `score` reports the best edge-alignment shift (dx,dy) and
its score. `offsets` runs the edge score across every matched frame at temporal
offsets -1/0/+1 (projection frame n vs screenshot n+t) and prints which wins --
the test for a one-frame lag between RAM9 and the screen.
"""
import sys, struct, zlib, os, re

PROJ_BG = (18, 18, 24)


def _unfilter(raw, w, h, bpp):
    stride = w * bpp
    out = bytearray(h * stride)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        ft = raw[pos]; pos += 1
        cur = bytearray(raw[pos:pos + stride]); pos += stride
        if ft == 1:
            for i in range(bpp, stride):
                cur[i] = (cur[i] + cur[i - bpp]) & 0xff
        elif ft == 2:
            for i in range(stride):
                cur[i] = (cur[i] + prev[i]) & 0xff
        elif ft == 3:
            for i in range(stride):
                a = cur[i - bpp] if i >= bpp else 0
                cur[i] = (cur[i] + ((a + prev[i]) >> 1)) & 0xff
        elif ft == 4:
            for i in range(stride):
                a = cur[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                cur[i] = (cur[i] + pr) & 0xff
        out[y * stride:(y + 1) * stride] = cur
        prev = cur
    return bytes(out)


def read_png(path):
    """Return (w, h, rgb-bytes) for 8-bit RGB/RGBA/grayscale PNGs."""
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", path + ": not a PNG"
    pos = 8
    w = h = 0; ctype = 0; bitd = 8
    idat = b""
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        data = d[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, bitd, ctype = struct.unpack(">IIBB", data[:10])
        elif typ == b"IDAT":
            idat += data
        elif typ == b"IEND":
            break
        pos += 12 + ln
    assert bitd == 8, "only 8-bit PNGs supported"
    ch = {0: 1, 2: 3, 6: 4}[ctype]
    raw = _unfilter(zlib.decompress(idat), w, h, ch)
    rgb = bytearray(w * h * 3)
    for i in range(w * h):
        if ctype == 2:
            rgb[i * 3:i * 3 + 3] = raw[i * 3:i * 3 + 3]
        elif ctype == 6:
            rgb[i * 3:i * 3 + 3] = raw[i * 4:i * 4 + 3]
        else:
            g = raw[i]; rgb[i * 3] = g; rgb[i * 3 + 1] = g; rgb[i * 3 + 2] = g
    return w, h, bytes(rgb)


def write_png(path, w, h, rgb):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3:(y + 1) * w * 3]
    comp = zlib.compress(bytes(raw), 9)
    def chunk(t, b):
        return (struct.pack(">I", len(b)) + t + b +
                struct.pack(">I", zlib.crc32(t + b) & 0xffffffff))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", comp))
        f.write(chunk(b"IEND", b""))


def gray(rgb, n):
    return [(rgb[i * 3] * 77 + rgb[i * 3 + 1] * 150 + rgb[i * 3 + 2] * 29) >> 8
            for i in range(n)]


def sobel(g, w, h):
    mag = [0.0] * (w * h)
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            i = y * w + x
            gx = (g[i - w + 1] + 2 * g[i + 1] + g[i + w + 1]
                  - g[i - w - 1] - 2 * g[i - 1] + -g[i + w - 1])
            gy = (g[i + w - 1] + 2 * g[i + w] + g[i + w + 1]
                  - g[i - w - 1] - 2 * g[i - w] - g[i - w + 1])
            mag[i] = (gx * gx + gy * gy) ** 0.5
    return mag


def proj_fg_edges(rgb, w, h):
    """List of (x, y) projection-foreground silhouette-edge pixels."""
    fg = [1 if (rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]) != PROJ_BG else 0
          for i in range(w * h)]
    pts = []
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            i = y * w + x
            if fg[i] and (not fg[i - 1] or not fg[i + 1]
                          or not fg[i - w] or not fg[i + w]):
                pts.append((x, y))
    return pts


def score_shift(smag, pts, w, h, dx, dy):
    s = 0.0; n = 0
    for (x, y) in pts:
        sx, sy = x + dx, y + dy
        if 0 <= sx < w and 0 <= sy < h:
            s += smag[sy * w + sx]
            n += 1
    return (s / n) if n else 0.0


def best_score(shot, proj, w, h, rng=18):
    smag = sobel(gray(shot, w * h), w, h)
    pts = proj_fg_edges(proj, w, h)
    best = (-1.0, 0, 0)
    for dy in range(-rng, rng + 1, 3):
        for dx in range(-rng, rng + 1, 3):
            sc = score_shift(smag, pts, w, h, dx, dy)
            if sc > best[0]:
                best = (sc, dx, dy)
    return best


def cmd_overlay(shot_p, proj_p, out_p, dx=0, dy=0):
    w, h, shot = read_png(shot_p)
    pw, ph, proj = read_png(proj_p)
    assert (pw, ph) == (w, h), f"size mismatch {pw}x{ph} vs {w}x{h}"
    out = bytearray(shot)
    edge = set(proj_fg_edges(proj, w, h))
    for y in range(h):
        for x in range(w):
            i = y * w + x
            r, g, b = proj[i * 3], proj[i * 3 + 1], proj[i * 3 + 2]
            sx, sy = x + dx, y + dy
            if not (0 <= sx < w and 0 <= sy < h):
                continue
            j = sy * w + sx
            if (r, g, b) != PROJ_BG:
                out[j * 3] = (out[j * 3] + r) >> 1
                out[j * 3 + 1] = (out[j * 3 + 1] + g) >> 1
                out[j * 3 + 2] = (out[j * 3 + 2] + b) >> 1
            if (x, y) in edge:                # solid silhouette edge
                out[j * 3] = 0; out[j * 3 + 1] = 255; out[j * 3 + 2] = 255
    write_png(out_p, w, h, bytes(out))
    print(f"overlay -> {out_p}  (shift dx={dx} dy={dy})")


def cmd_score(shot_p, proj_p):
    w, h, shot = read_png(shot_p)
    pw, ph, proj = read_png(proj_p)
    assert (pw, ph) == (w, h)
    sc, dx, dy = best_score(shot, proj, w, h)
    print(f"best edge-align score={sc:.1f} at dx={dx} dy={dy}")


def _index(name):
    m = re.match(r"(?:proj_)?f(\d+)", os.path.basename(name))
    return int(m.group(1)) if m else None


def cmd_offsets(projdir, shotdir):
    projs = {}
    for f in os.listdir(projdir):
        if f.endswith(".png"):
            i = _index(f)
            if i is not None:
                projs[i] = os.path.join(projdir, f)
    shots = {}
    for f in os.listdir(shotdir):
        if f.endswith(".png"):
            i = _index(f)
            if i is not None:
                shots[i] = os.path.join(shotdir, f)
    totals = {-1: [], 0: [], 1: []}
    for i in sorted(projs):
        pw, ph, proj = read_png(projs[i])
        for t in (-1, 0, 1):
            if (i + t) not in shots:
                continue
            w, h, shot = read_png(shots[i + t])
            if (w, h) != (pw, ph):
                continue
            sc, dx, dy = best_score(shot, proj, w, h)
            totals[t].append(sc)
    print("temporal offset (proj n vs screen n+t): mean best edge-align score")
    for t in (-1, 0, 1):
        v = totals[t]
        print(f"  t={t:+d}: mean={sum(v)/len(v):.1f} over {len(v)} frames"
              if v else f"  t={t:+d}: no frames")
    best_t = max(totals, key=lambda k: (sum(totals[k]) / len(totals[k]))
                 if totals[k] else -1)
    print(f"  -> best temporal offset: t={best_t:+d}")


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    c = sys.argv[1]
    if c == "overlay":
        dx = int(sys.argv[5]) if len(sys.argv) > 5 else 0
        dy = int(sys.argv[6]) if len(sys.argv) > 6 else 0
        cmd_overlay(sys.argv[2], sys.argv[3], sys.argv[4], dx, dy)
    elif c == "score":
        cmd_score(sys.argv[2], sys.argv[3])
    elif c == "offsets":
        cmd_offsets(sys.argv[2], sys.argv[3])
    else:
        print(__doc__); sys.exit(1)


if __name__ == "__main__":
    main()
