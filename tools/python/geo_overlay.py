#!/usr/bin/env python3
"""Overlay a geo-project projection onto the matching MAME screenshot and score
the alignment. Local analysis helper for the geometriser pipeline -- it reads
only local capture PNGs (never committed) and the projection PNGs written by
`vf2i960 geo-project`. Pure stdlib (zlib only), no third-party deps.

Usage:
  geo_overlay.py overlay <screenshot.png> <proj.png> <out_overlay.png> [dx dy]
  geo_overlay.py score   <screenshot.png> <proj.png>
  geo_overlay.py offsets <projdir> <screendir>
  geo_overlay.py objerr  <proj.png.csv> [W H]
  geo_overlay.py fighters <screenshot.png> <proj.png.tris> <proj.png.csv> <out.png>
  geo_overlay.py fitfighters <shotdir> <frameidx> <proj.png.tris> <proj.png.csv> <ref_f00.csv>

`overlay` blends the projection over the screenshot (projection colours kept:
red anchor, grey other, dark shadow) and draws its silhouette edges, so the two
can be compared by eye. `score` reports the best edge-alignment shift (dx,dy) and
its score. `offsets` runs the edge score across every matched frame at temporal
offsets -1/0/+1 (projection frame n vs screenshot n+t) and prints which wins --
the test for a one-frame lag between RAM9 and the screen. `objerr` reads the
per-object CSV written by `geo-project` and reports, per object, an objective
placement error: how far its projected bbox spills off the [0,W]x[0,H] canvas,
whether it is degenerate (a vertex projected near Z=0, bbox exploding beyond
2x the canvas), and its horizontal offset from the fighter cluster it belongs
to. It splits the non-shadow fighter objects into two clusters at the widest
gap in projected x and reports each cluster's centre and separation -- the
measure of the two fighters' on-screen spread.
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


def shot_shadow_centroids(shot, w, h):
    """Left/right dark-floor (shadow) centroids in the screenshot: very dark
    pixels in the lower floor band, split at screen centre."""
    L = [0, 0, 0]; R = [0, 0, 0]
    for y in range(int(h * 0.80), h):           # lower ~20%: floor + shadows
        for x in range(w):
            i = y * w + x
            lum = (shot[i*3]*77 + shot[i*3+1]*150 + shot[i*3+2]*29) >> 8
            if lum < 70:
                t = L if x < w // 2 else R
                t[0] += x; t[1] += y; t[2] += 1
    cl = (L[0]/L[2], L[1]/L[2]) if L[2] else None
    cr = (R[0]/R[2], R[1]/R[2]) if R[2] else None
    return cl, cr


def proj_shadow_centroids(proj, w, h):
    """Left/right projection shadow (colour 40,40,52) centroids."""
    L = [0, 0, 0]; R = [0, 0, 0]
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if (proj[i*3], proj[i*3+1], proj[i*3+2]) == (40, 40, 52):
                t = L if x < w // 2 else R
                t[0] += x; t[1] += y; t[2] += 1
    cl = (L[0]/L[2], L[1]/L[2]) if L[2] else None
    cr = (R[0]/R[2], R[1]/R[2]) if R[2] else None
    return cl, cr


def cmd_fit(projdir, shotdir):
    import math
    projs = {}; shots = {}
    for f in os.listdir(projdir):
        i = _index(f)
        if f.endswith(".png") and i is not None and "overlay" not in f:
            projs[i] = os.path.join(projdir, f)
    for f in os.listdir(shotdir):
        i = _index(f)
        if f.endswith(".png") and i is not None:
            shots[i] = os.path.join(shotdir, f)
    frames = [0, 10, 20, 29]
    print("per-frame shadow-landmark correspondences (proj -> screen):")
    pts = []  # (proj_x, proj_y, shot_x, shot_y)
    for i in frames:
        if i not in projs or i not in shots:
            continue
        pw, ph, proj = read_png(projs[i])
        w, h, shot = read_png(shots[i])
        pl, pr = proj_shadow_centroids(proj, pw, ph)
        sl, sr = shot_shadow_centroids(shot, w, h)
        for (p, s, nm) in ((pl, sl, "L"), (pr, sr, "R")):
            if p and s:
                pts.append((p[0], p[1], s[0], s[1]))
                print(f"  f{i:02d} {nm}: proj({p[0]:.0f},{p[1]:.0f}) -> "
                      f"screen({s[0]:.0f},{s[1]:.0f})  d=({s[0]-p[0]:+.0f},"
                      f"{s[1]-p[1]:+.0f})")
    if len(pts) < 2:
        print("not enough landmarks"); return
    n = len(pts)
    # translation-only LS: dx,dy = mean(screen - proj)
    dx = sum(s[2] - s[0] for s in pts) / n
    dy = sum(s[3] - s[1] for s in pts) / n
    res_t = (sum((s[2]-s[0]-dx)**2 + (s[3]-s[1]-dy)**2 for s in pts) / n) ** .5
    print(f"\ntranslation-only LS: dx={dx:+.1f} dy={dy:+.1f}  "
          f"residual RMS={res_t:.1f}px")
    # translation + uniform scale about projection centroid:
    mpx = sum(s[0] for s in pts)/n; mpy = sum(s[1] for s in pts)/n
    msx = sum(s[2] for s in pts)/n; msy = sum(s[3] for s in pts)/n
    num = sum((s[0]-mpx)*(s[2]-msx)+(s[1]-mpy)*(s[3]-msy) for s in pts)
    den = sum((s[0]-mpx)**2+(s[1]-mpy)**2 for s in pts)
    sc = num/den if den else 1.0
    tx = msx - sc*mpx; ty = msy - sc*mpy
    res_s = (sum((sc*s[0]+tx-s[2])**2 + (sc*s[1]+ty-s[3])**2
                 for s in pts)/n) ** .5
    print(f"translation+scale LS: scale={sc:.3f} tx={tx:+.1f} ty={ty:+.1f}  "
          f"residual RMS={res_s:.1f}px")
    print(f"  (current geo-project xoff/yoff -> add dx,dy to xoff,yoff for a "
          f"translation fix)")


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


def _csv_rows(path):
    import csv
    return list(csv.DictReader(open(path, newline="")))


def _fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def cmd_objerr(csv_path, W=496, H=384):
    rows = _csv_rows(csv_path)
    objs = []
    for r in rows:
        objs.append(dict(
            obj=int(r["obj"]), rec=r["rec_byte"], group=r["group"],
            side=r["side"], mode3=r["mode3"],
            cx=_fnum(r["cx"]), cy=_fnum(r["cy"]),
            minx=_fnum(r["minx"]), miny=_fnum(r["miny"]),
            maxx=_fnum(r["maxx"]), maxy=_fnum(r["maxy"]),
            h=_fnum(r["screen_h"]), ntri=int(r["ntri"])))

    def off_canvas(o):
        # pixels the bbox spills beyond the visible [0,W]x[0,H] canvas
        if o["minx"] is None:
            return 0.0
        return max(0.0, -o["minx"]) + max(0.0, o["maxx"] - W) \
            + max(0.0, -o["miny"]) + max(0.0, o["maxy"] - H)

    def degenerate(o):
        # a vertex near Z=0 blows the projection far past the canvas
        return o["minx"] is not None and (
            o["minx"] < -W or o["maxx"] > 2 * W
            or o["miny"] < -H or o["maxy"] > 2 * H)

    def onscreen(o):
        return o["cx"] is not None and o["cx"] >= 0

    print(f"# per-object placement error  canvas={W}x{H}  ({csv_path})")
    for grp in ("decor", "fighter", "shadow"):
        g = [o for o in objs if o["group"] == grp]
        if not g:
            continue
        deg = [o for o in g if degenerate(o)]
        spill = [o for o in g if off_canvas(o) > 0 and not degenerate(o)]
        print(f"{grp}: {len(g)} objs | degenerate(Z~0) {len(deg)} | "
              f"bbox spills off-canvas {len(spill)}")

    # cluster on the solid body pieces only (fully on-canvas, not degenerate):
    # off-canvas / Z~0 outliers would otherwise forge a spurious gap.
    fset = [o for o in objs if o["group"] == "fighter" and onscreen(o)
            and not degenerate(o) and off_canvas(o) == 0]
    fset.sort(key=lambda o: o["cx"])
    if len(fset) >= 2:
        # split into two fighters at the widest gap in projected x
        gaps = [(fset[i + 1]["cx"] - fset[i]["cx"], i)
                for i in range(len(fset) - 1)]
        gap, gi = max(gaps)
        left = fset[:gi + 1]
        right = fset[gi + 1:]

        def wcen(c):
            tw = sum(o["ntri"] for o in c)
            return sum(o["cx"] * o["ntri"] for o in c) / tw if tw else 0.0
        clx, crx = wcen(left), wcen(right)
        print(f"\nfighter clusters (split at widest x-gap {gap:.0f}px):")
        print(f"  cluster A (left) : {len(left):2d} objs  centre cx={clx:.0f}  "
              f"x[{left[0]['cx']:.0f}..{left[-1]['cx']:.0f}]")
        print(f"  cluster B (right): {len(right):2d} objs  centre cx={crx:.0f}  "
              f"x[{right[0]['cx']:.0f}..{right[-1]['cx']:.0f}]")
        print(f"  on-screen separation A<->B: {crx - clx:.0f}px")

    worst = sorted(
        [o for o in objs if o["group"] == "fighter"],
        key=lambda o: (degenerate(o), off_canvas(o)), reverse=True)
    print("\nworst-placed fighter objects (degenerate / off-canvas first):")
    print("  obj      rec  m    cx     cy   off_canvas  bbox_x            flags")
    for o in worst[:8]:
        fl = []
        if degenerate(o):
            fl.append("DEGEN(Z~0)")
        if o["maxx"] is not None and o["maxx"] > W:
            fl.append("OVERFLOW_R")
        cx = -1.0 if o["cx"] is None else o["cx"]
        cy = -1.0 if o["cy"] is None else o["cy"]
        bx = "n/a" if o["minx"] is None else f"[{o['minx']:.0f}..{o['maxx']:.0f}]"
        print(f"  {o['obj']:>3} {o['rec']:>9} m{o['mode3']} {cx:6.1f} {cy:6.1f}"
              f"  {off_canvas(o):9.0f}  {bx:17s} {' '.join(fl)}")


def _putpx(buf, w, h, x, y, col):
    x = int(round(x)); y = int(round(y))
    if 0 <= x < w and 0 <= y < h:
        i = (y * w + x) * 3
        buf[i] = col[0]; buf[i + 1] = col[1]; buf[i + 2] = col[2]


def _disk(buf, w, h, x, y, r, col):
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy <= r * r:
                _putpx(buf, w, h, x + dx, y + dy, col)


def _line(buf, w, h, x0, y0, x1, y1, col, r=1):
    # clip trivially-huge endpoints so off-canvas spill does not explode the loop
    def cl(v, lo, hi):
        return lo if v < lo else hi if v > hi else v
    x0 = cl(x0, -2 * w, 3 * w); x1 = cl(x1, -2 * w, 3 * w)
    y0 = cl(y0, -2 * h, 3 * h); y1 = cl(y1, -2 * h, 3 * h)
    x0 = int(round(x0)); y0 = int(round(y0))
    x1 = int(round(x1)); y1 = int(round(y1))
    dx = abs(x1 - x0); dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        _disk(buf, w, h, x0, y0, r, col)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy; x0 += sx
        if e2 <= dx:
            err += dx; y0 += sy


def _cross(buf, w, h, x, y, size, col, r=1):
    if x is None or y is None:
        return
    _line(buf, w, h, x - size, y, x + size, y, (255, 255, 255), r + 1)
    _line(buf, w, h, x, y - size, x, y + size, (255, 255, 255), r + 1)
    _line(buf, w, h, x - size, y, x + size, y, col, r)
    _line(buf, w, h, x, y - size, x, y + size, col, r)


def cmd_fighters(shot_p, tris_p, csv_p, out_p, W=0, H=0):
    w, h, shot = read_png(shot_p)
    if W and H and (W, H) != (w, h):
        print(f"note: screenshot {w}x{h} != requested {W}x{H}")
    # dim the screenshot so the overlay reads clearly
    buf = bytearray(len(shot))
    for i in range(len(shot)):
        buf[i] = (shot[i] * 42) // 100

    rows = _csv_rows(csv_p)
    fcx = {}       # obj -> (cx, cy, ntri) for on-canvas fighters
    shadow_c = []  # projected shadow centres
    for r in rows:
        o = int(r["obj"]); grp = r["group"]
        cx = _fnum(r["cx"]); cy = _fnum(r["cy"])
        minx = _fnum(r["minx"]); maxx = _fnum(r["maxx"])
        miny = _fnum(r["miny"]); maxy = _fnum(r["maxy"])
        deg = (minx is not None and (minx < -w or maxx > 2 * w
               or miny < -h or maxy > 2 * h))
        spill = (minx is not None and (minx < 0 or maxx > w
                 or miny < 0 or maxy > h))
        if grp == "shadow" and cx is not None and cx >= 0:
            shadow_c.append((cx, cy))
        if grp == "fighter" and cx is not None and cx >= 0 and not deg \
                and not spill:
            fcx[o] = (cx, cy, int(r["ntri"]))

    # split the solid fighters into two clusters at the widest x-gap
    items = sorted(fcx.items(), key=lambda kv: kv[1][0])
    split = None
    if len(items) >= 2:
        gaps = [(items[i + 1][1][0] - items[i][1][0], i)
                for i in range(len(items) - 1)]
        _, gi = max(gaps)
        split = 0.5 * (items[gi][1][0] + items[gi + 1][1][0])
        clusterA = set(o for o, v in items if v[0] <= split)
    else:
        clusterA = set(fcx)

    def side_of(o, px_mid):
        # cluster for any fighter triangle; fall back to x vs the split line
        if o in clusterA:
            return "A"
        if o in fcx:
            return "B"
        return "A" if (split is not None and px_mid <= split) else "B"

    COL_A = (80, 150, 255)    # left group  (Akira)
    COL_B = (255, 140, 40)    # right group (Lau)

    # thick fighter contours, coloured by cluster
    ntr = 0
    with open(tris_p) as f:
        next(f, None)
        for ln in f:
            p = ln.rstrip("\n").split(",")
            if len(p) < 9 or p[2] != "fighter":
                continue
            o = int(p[0])
            xs = [float(p[3]), float(p[5]), float(p[7])]
            ys = [float(p[4]), float(p[6]), float(p[8])]
            col = COL_A if side_of(o, sum(xs) / 3.0) == "A" else COL_B
            for a, b in ((0, 1), (1, 2), (2, 0)):
                _line(buf, w, h, xs[a], ys[a], xs[b], ys[b], col, r=1)
            ntr += 1

    # group centres (ntri-weighted) from the clean fighter set
    def centre(members):
        tw = sum(fcx[o][2] for o in members if o in fcx)
        if not tw:
            return None, None
        cx = sum(fcx[o][0] * fcx[o][2] for o in members if o in fcx) / tw
        cy = sum(fcx[o][1] * fcx[o][2] for o in members if o in fcx) / tw
        return cx, cy
    ax, ay = centre(clusterA)
    bx, by = centre(set(fcx) - clusterA)
    _cross(buf, w, h, ax, ay, 11, COL_A, r=2)
    _cross(buf, w, h, bx, by, 11, COL_B, r=2)

    # projected shadow centres (yellow) and screen shadow centres (green)
    for (sx, sy) in shadow_c:
        _cross(buf, w, h, sx, sy, 5, (255, 230, 0), r=1)
    sl, sr = shot_shadow_centroids(shot, w, h)
    for c in (sl, sr):
        if c:
            _cross(buf, w, h, c[0], c[1], 7, (0, 255, 120), r=1)

    write_png(out_p, w, h, buf)
    print(f"fighters overlay -> {out_p} ({w}x{h}); {ntr} fighter tris, "
          f"{len(fcx)} solid fighter objs, {len(shadow_c)} shadow centres")
    print(f"  cluster A (left, blue)  centre=({ax:.0f},{ay:.0f})"
          if ax is not None else "  cluster A: empty")
    print(f"  cluster B (right, orange) centre=({bx:.0f},{by:.0f})"
          if bx is not None else "  cluster B: empty")
    print(f"  screen shadows (green): L={sl} R={sr}")


EXCLUDE_RECS = ("0x581d50", "0x1a9b54")


def _rec_side_map(ref_csv):
    """rec -> 'L'/'R' from a reference frame's CSV, by projected-matrix Tx sign;
    ties dropped (records a fighter shares). Needs the `tx` column (geo-project)."""
    tally = {}
    for r in _csv_rows(ref_csv):
        if r["group"] != "fighter" or r["rec_byte"] in EXCLUDE_RECS:
            continue
        tx = _fnum(r.get("tx"))
        if tx is None:
            continue
        t = tally.setdefault(r["rec_byte"], [0, 0])
        t[0 if tx < 0 else 1] += 1
    side = {}
    for rec, (nl, nr) in tally.items():
        if nl > nr:
            side[rec] = "L"
        elif nr > nl:
            side[rec] = "R"
    return side


def _fill_tris(tris, w, h):
    """Rasterize a list of (x0,y0,x1,y1,x2,y2) to a filled 0/1 mask."""
    mask = bytearray(w * h)
    for (x0, y0, x1, y1, x2, y2) in tris:
        minx = max(0, int(min(x0, x1, x2)))
        maxx = min(w - 1, int(max(x0, x1, x2)) + 1)
        miny = max(0, int(min(y0, y1, y2)))
        maxy = min(h - 1, int(max(y0, y1, y2)) + 1)
        if maxx < minx or maxy < miny:
            continue
        d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(d) < 1e-9:
            continue
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                a = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / d
                b = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / d
                c = 1.0 - a - b
                if a >= -0.01 and b >= -0.01 and c >= -0.01:
                    mask[y * w + x] = 1
    return mask


def _boundary(mask, w, h):
    pts = []
    for y in range(1, h - 1):
        row = y * w
        for x in range(1, w - 1):
            if mask[row + x] and not (mask[row + x - 1] and mask[row + x + 1]
                                      and mask[row - w + x] and mask[row + w + x]):
                pts.append((x, y))
    return pts


def _motion_mask(shotdir, idx, thr=28):
    """Foreground = pixels that move between this screenshot and its neighbours
    (isolates the fighters from the static stage). Returns (w,h,mask 0/1)."""
    import os, re
    files = {}
    for fn in os.listdir(shotdir):
        m = re.match(r"f(\d+)", fn)
        if m and fn.lower().endswith(".png"):
            files[int(m.group(1))] = os.path.join(shotdir, fn)
    w, h, cur = read_png(files[idx])
    gc_ = gray(cur, w * h)
    mask = bytearray(w * h)
    for nb in (idx - 1, idx + 1):
        if nb not in files:
            continue
        w2, h2, o = read_png(files[nb])
        if (w2, h2) != (w, h):
            continue
        go = gray(o, w * h)
        for i in range(w * h):
            if abs(gc_[i] - go[i]) > thr:
                mask[i] = 1
    return w, h, mask


def cmd_fitfighters(shotdir, idx, tris_p, csv_p, ref_csv, rng=100):
    idx = int(idx)
    w, h, fg = _motion_mask(shotdir, idx)
    recside = _rec_side_map(ref_csv)
    side_tris = {"L": [], "R": []}
    with open(tris_p) as f:
        next(f, None)
        for ln in f:
            p = ln.rstrip("\n").split(",")
            if len(p) < 9 or p[2] != "fighter" or p[1] in EXCLUDE_RECS:
                continue
            sd = recside.get(p[1])
            if sd is None:
                continue
            side_tris[sd].append(tuple(float(v) for v in p[3:9]))
    nfg = sum(fg)
    print(f"# fitfighters frame {idx} ({shotdir})  motion-fg {nfg}px")
    for side, who in (("L", "Akira"), ("R", "Lau")):
        tris = side_tris[side]
        if not tris:
            print(f"  {side}/{who}: no triangles")
            continue
        mask = _fill_tris(tris, w, h)
        pts = [(x, y) for y in range(h) for x in range(w) if mask[y * w + x]]
        if len(pts) > 400:
            step = len(pts) // 400 + 1
            pts = pts[::step]
        best = (-1.0, 0, 0)
        for dy in range(-rng, rng + 1):
            for dx in range(-rng, rng + 1):
                s = 0
                for (x, y) in pts:
                    xx = x + dx; yy = y + dy
                    if 0 <= xx < w and 0 <= yy < h and fg[yy * w + xx]:
                        s += 1
                if s > best[0]:
                    best = (s, dx, dy)
        sc, dx, dy = best
        s0 = sum(1 for (x, y) in pts
                 if fg[y * w + x]) if pts else 0
        print(f"  {side}/{who}: best shift dx={dx:+d} dy={dy:+d}  "
              f"silhouette-in-motion {100*sc/len(pts):.0f}% "
              f"(vs {100*s0/len(pts):.0f}% at 0,0); {len(tris)} tris, "
              f"{len(pts)} fill pts")


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
    elif c == "fit":
        cmd_fit(sys.argv[2], sys.argv[3])
    elif c == "objerr":
        w = int(sys.argv[3]) if len(sys.argv) > 4 else 496
        h = int(sys.argv[4]) if len(sys.argv) > 4 else 384
        cmd_objerr(sys.argv[2], w, h)
    elif c == "fighters":
        cmd_fighters(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    elif c == "fitfighters":
        cmd_fitfighters(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5],
                        sys.argv[6])
    else:
        print(__doc__); sys.exit(1)


if __name__ == "__main__":
    main()
