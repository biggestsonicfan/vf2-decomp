#!/usr/bin/env python3
"""Camera-lag test for the VF2 fighter body matrices.

Hypothesis (kazuyette): the fighters' upright BODY matrices are composed with the
PREVIOUS frame's camera, while the decor and the fighters' SHADOW matrices use the
CURRENT frame's camera. If so, the body-vs-shadow horizontal gap seen in a single
frame should TRACK the camera's per-frame motion, and re-composing each body matrix
with cam(n) in place of cam(n-1) should drop the bodies back onto their shadows.

This walks every frame of a local RAM9 capture (never committed), tracks each
object's absolute 0x0b matrix (MAME-exact, no stack), classifies decor / fighter /
shadow exactly as geo-project/geo-scene do, excludes the two decor-matrix records
0x581d50 / 0x1a9b54, and reports per frame:
  - cam motion   : the decor (shared) matrix translation change n-1 -> n
  - body-shadow  : the per-fighter gap in Tx,Tz and in projected screen x
  - predicted    : the gap the camera-lag hypothesis predicts (cam(n-1) vs cam(n))
  - recomposed   : the body-shadow screen gap after swapping cam(n-1)->cam(n)

Usage:
  geo_camtest.py <ram9.bin> <readstarts.txt>              # camera-lag test
  geo_camtest.py track <ram9.bin> <readstarts.txt> <shotdir>
  geo_camtest.py rtrack <ram9.bin> <readstarts.txt> <shotdir>
  geo_camtest.py addrmap <ram9.bin> <readstarts.txt> [pose_frame]
  geo_camtest.py coprochk <ram9_writes.bin> <writes.txt>

`coprochk` verifies the TGP copro's RAM9 writes (the C lines of a write-trace
capture) against the 0x0b fighter matrices in each frame's snapshot: it confirms
the matrices we read are the copro's genuine per-frame output, not stale/corrupt.

`rtrack` is `track` but each object is bound to its fighter by its RECORD (frozen
from frame 0 by Tx sign, ties dropped), not by its live position -- so an advancing
fighter's limbs do not get re-sorted into the other fighter late in the sequence.

`addrmap` gives, for each body/shadow object of a frame, the RAM9 byte address of
its 0x0b matrix command and its 0x01 object command and their 32 KiB zone relative
to the frame's readstart, lists the walk's jumps (flagging cross-zone ones), and
then tabulates across all frames whether the body matrix addresses stay fixed while
the readstart rotates through the 4 zones -- the test for a shared/stale zone.

`track` traces, per fighter and per frame, three screen-x positions -- projected
body centre, projected shadow centre, and the real screenshot shadow (the green
cross = `geo_overlay.shot_shadow_centroids`) -- and reports which projection
(body or shadow) stays closest to the real fighter and whether the body matrices
move frame-to-frame or stay frozen.

Pure stdlib (track imports read_png/shot_shadow_centroids from geo_overlay.py). Convention: a matrix is 12 floats m[0..11] applied as MAME
transform_point  p' = p . A + T,  A[i][j]=m[3i+j] (i,j in 0..2), T=m[9..11].
Projection (VF2): fx=fy=600, screen_x = 84 + 248 + Tx*fx/Tz.
"""
import sys, struct

FX = FY = 600.0
XOFF, YOFF, CENX, CENY = 84.0, 130.0, 248.0, 319.0
SHADOW_EPS = 0.10
EXCLUDE = (0x581d50, 0x1a9b54)
FRAME_BYTES = 131072


def f32(u):
    return struct.unpack("<f", struct.pack("<I", u))[0]


def need(w, cmd, ptr):
    if cmd == 0:
        return 0
    if cmd in (0x07, 0x17, 0x08, 0x18, 0x10, 0x16, 0x1e):
        return 1
    if cmd in (0x09, 0x19, 0x0d):
        return 2
    if cmd in (0x0a, 0x1a, 0x0c, 0x1c):
        return 3
    if cmd in (0x03, 0x13):
        return 6
    if cmd in (0x0b, 0x1b):
        return 12
    if cmd in (0x01, 0x11):
        return 4
    if cmd in (0x04, 0x14, 0x05, 0x15):
        return 2 + w[ptr + 1]
    if cmd == 0x06:
        return 2 + w[ptr + 1] * 2
    if cmd == 0x1d:
        return 2 + w[ptr + 1] * 3
    if cmd in (0x02, 0x12):
        p = ptr + 8
        while True:
            a = w[p]; p += 1
            if (a & 3) == 0:
                break
            p += 5
            if a & 1:
                p += 3
        return p - ptr
    if cmd == 0x0e:
        return 32 + 1 + w[ptr + 32] * 3
    return 0


def det3(m):
    return (m[0] * (m[4] * m[8] - m[5] * m[7])
            - m[3] * (m[1] * m[8] - m[2] * m[7])
            + m[6] * (m[1] * m[5] - m[2] * m[4]))


def norm(a, b, c):
    return (a * a + b * b + c * c) ** 0.5


def flatness(m):
    n0 = norm(m[0], m[1], m[2])
    n1 = norm(m[3], m[4], m[5])
    n2 = norm(m[6], m[7], m[8])
    d = n0 * n1 * n2
    return abs(det3(m)) / d if d > 1e-12 else 0.0


def A_of(m):
    return [[m[0], m[1], m[2]], [m[3], m[4], m[5]], [m[6], m[7], m[8]]]


def T_of(m):
    return [m[9], m[10], m[11]]


def inv3(A):
    d = (A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
         - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
         + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]))
    if abs(d) < 1e-20:
        return None
    c = [[(A[1][1] * A[2][2] - A[1][2] * A[2][1]) / d,
          (A[0][2] * A[2][1] - A[0][1] * A[2][2]) / d,
          (A[0][1] * A[1][2] - A[0][2] * A[1][1]) / d],
         [(A[1][2] * A[2][0] - A[1][0] * A[2][2]) / d,
          (A[0][0] * A[2][2] - A[0][2] * A[2][0]) / d,
          (A[0][2] * A[1][0] - A[0][0] * A[1][2]) / d],
         [(A[1][0] * A[2][1] - A[1][1] * A[2][0]) / d,
          (A[0][1] * A[2][0] - A[0][0] * A[2][1]) / d,
          (A[0][0] * A[1][1] - A[0][1] * A[1][0]) / d]]
    return c


def matmul(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)]
            for i in range(3)]


def vrowmat(v, A):              # row vector v times matrix A
    return [sum(v[k] * A[k][j] for k in range(3)) for j in range(3)]


def vadd(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def swap_AT(dec_prev, dec_cur):
    """S = Ddec_cur o Ddec_prev^-1 (apply prev^-1 first). Returns (A_s, T_s)
    so that a point p maps p -> p.A_s + T_s."""
    Ap, Tp = A_of(dec_prev), T_of(dec_prev)
    Ac, Tc = A_of(dec_cur), T_of(dec_cur)
    Api = inv3(Ap)
    if Api is None:
        return None
    Tpi = [-x for x in vrowmat(Tp, Api)]   # inverse translation
    A_s = matmul(Api, Ac)
    T_s = vadd(vrowmat(Tpi, Ac), Tc)
    return A_s, T_s


def apply_pt(T, AT):
    A_s, T_s = AT
    return vadd(vrowmat(T, A_s), T_s)


def screen_x(T):
    return XOFF + CENX + (T[0] * FX) / T[2] if abs(T[2]) > 1e-6 else None


def walk(w, start_word):
    """Return list of (matrix12, rec) for every object, in list order."""
    objs = []
    mat = [0.0] * 12
    ptr = start_word
    opc = 0
    nw = len(w)
    while opc < 0x40000:
        if ptr >= nw:
            break
        op = w[ptr]; ptr += 1; opc += 1
        if op & 0x80000000:
            ptr = (op & 0x1ffff) >> 2
            continue
        cmd = (op >> 23) & 0x1f
        if cmd in (0x0b, 0x1b):
            mat = [f32(w[ptr + k]) for k in range(12)]
        elif cmd in (0x0c, 0x1c):
            mat = mat[:9] + [f32(w[ptr]), f32(w[ptr + 1]), f32(w[ptr + 2])]
        elif cmd in (0x01, 0x11):
            oba = w[ptr + 2]
            rec = (oba & 0x7fffff) * 4
            objs.append((list(mat), rec))
        if cmd in (0x0f, 0x1f):
            break
        ptr += need(w, cmd, ptr)
    return objs


ZONE = 0x8000   # 32 KiB RAM9 bank


def walk_detailed(w, start_word):
    """Like walk() but records, per object, the byte address of the 0x0b matrix
    command that set its current matrix and of its own 0x01 command, plus the
    list of jumps (src_byte, dst_byte). Returns (objs, jumps) where each obj is
    a dict(mat, rec, mat_addr, obj_addr)."""
    objs = []
    jumps = []
    mat = [0.0] * 12
    mat_addr = -1
    ptr = start_word
    opc = 0
    nw = len(w)
    while opc < 0x40000:
        if ptr >= nw:
            break
        op = w[ptr]; op_addr = ptr * 4; ptr += 1; opc += 1
        if op & 0x80000000:
            dst = (op & 0x1ffff) >> 2
            jumps.append((op_addr, dst * 4))
            ptr = dst
            continue
        cmd = (op >> 23) & 0x1f
        if cmd in (0x0b, 0x1b):
            mat = [f32(w[ptr + k]) for k in range(12)]
            mat_addr = op_addr
        elif cmd in (0x0c, 0x1c):
            mat = mat[:9] + [f32(w[ptr]), f32(w[ptr + 1]), f32(w[ptr + 2])]
            mat_addr = op_addr
        elif cmd in (0x01, 0x11):
            oba = w[ptr + 2]
            rec = (oba & 0x7fffff) * 4
            objs.append(dict(mat=list(mat), rec=rec, mat_addr=mat_addr,
                             obj_addr=op_addr))
        if cmd in (0x0f, 0x1f):
            break
        ptr += need(w, cmd, ptr)
    return objs, jumps


def classify(mat, rec):
    if 0x3c1fac <= rec <= 0x3ca730:
        return "decor"
    if flatness(mat) < SHADOW_EPS:
        return "shadow"
    return "fighter"


def centroid_T(objs):
    if not objs:
        return None
    n = len(objs)
    return [sum(o[0][9 + k] for o in objs) / n for k in range(3)]


def load_frames(binp, txtp):
    """Return a list[frame] of {"decor","fighter","shadow": [(matrix12, rec)]}."""
    starts = {}
    for ln in open(txtp):
        p = ln.split()
        if len(p) >= 4 and p[0] == "F":
            try:
                starts[int(p[1])] = int(p[3], 16)
            except ValueError:
                pass
    data = open(binp, "rb").read()
    nfr = len(data) // FRAME_BYTES
    frames = []
    for fr in range(nfr):
        raw = data[fr * FRAME_BYTES:(fr + 1) * FRAME_BYTES]
        w = list(struct.unpack("<%dI" % (FRAME_BYTES // 4), raw))
        start = (starts.get(fr, 0) & 0x1ffff) >> 2
        objs = walk(w, start)
        groups = {"decor": [], "fighter": [], "shadow": []}
        for mat, rec in objs:
            if rec in EXCLUDE:
                continue
            groups[classify(mat, rec)].append((mat, rec))
        frames.append(groups)
    return frames


def cmd_camtest(binp, txtp):
    frames = load_frames(binp, txtp)
    nfr = len(frames)
    print(f"# camera-lag test  ({binp}), {nfr} frames, excluding "
          f"{', '.join('0x%x' % r for r in EXCLUDE)}")
    print("# per fighter side (L: body Tx<0, R: Tx>=0); gaps in screen px "
          "unless noted")
    print()
    hdr = ("frm  camdTx  camPanPx | side  bodyX  shdwX  gap  predGap  "
           "recGap")
    print(hdr)
    print("-" * len(hdr))

    agg = {"gap": [], "pred": [], "rec": [], "cam": []}
    for fr in range(1, nfr):
        dec_cur = frames[fr]["decor"]
        dec_prev = frames[fr - 1]["decor"]
        if not dec_cur or not dec_prev:
            continue
        Dc = dec_cur[0][0]
        Dp = dec_prev[0][0]
        camdTx = Dc[9] - Dp[9]
        sw = swap_AT(Dp, Dc)
        # camera pan in px: how a fixed world ref (the decor origin image) moves
        refT = T_of(Dc)
        panpx = None
        if sw:
            # a point sitting at the decor translation this frame, seen one frame
            # earlier -> difference is the camera's per-frame image motion
            back = apply_pt(refT, swap_AT(Dc, Dp)) if swap_AT(Dc, Dp) else None
            if back and screen_x(refT) is not None and screen_x(back) is not None:
                panpx = screen_x(back) - screen_x(refT)

        bodies = frames[fr]["fighter"]
        shadows = frames[fr]["shadow"]
        for side, sel in (("L", lambda t: t[0] < 0), ("R", lambda t: t[0] >= 0)):
            b = [o for o in bodies if sel(o[0][9:12])]
            s = [o for o in shadows if sel(o[0][9:12])]
            Tb = centroid_T(b)
            Ts = centroid_T(s)
            if Tb is None or Ts is None:
                continue
            bx = screen_x(Tb); sx = screen_x(Ts)
            if bx is None or sx is None:
                continue
            gap = bx - sx
            # recompose body with cam(n) instead of cam(n-1)
            recgap = predgap = None
            if sw:
                Tb_fix = apply_pt(Tb, sw)
                bxf = screen_x(Tb_fix)
                if bxf is not None:
                    recgap = bxf - sx
                # predicted gap: project the shadow root under cam(n-1) vs cam(n)
                swinv = swap_AT(Dc, Dp)
                if swinv:
                    Ts_prev = apply_pt(Ts, swinv)
                    sxp = screen_x(Ts_prev)
                    if sxp is not None:
                        predgap = sxp - sx
            agg["gap"].append(gap)
            if predgap is not None:
                agg["pred"].append(predgap)
            if recgap is not None:
                agg["rec"].append(recgap)
            agg["cam"].append(camdTx)
            print(f"{fr:3d}  {camdTx:+6.3f}  "
                  f"{panpx:+7.1f} | {side}    {bx:5.0f}  {sx:5.0f}  "
                  f"{gap:+5.0f}  {('%+.0f' % predgap) if predgap is not None else '   .':>6}  "
                  f"{('%+.0f' % recgap) if recgap is not None else '   .':>6}")

    def med(xs):
        xs = sorted(xs)
        return xs[len(xs) // 2] if xs else float("nan")

    def mae(xs):
        return sum(abs(x) for x in xs) / len(xs) if xs else float("nan")

    print("\nsummary (median / mean-abs over all frame x side):")
    print(f"  raw body-shadow gap : median {med(agg['gap']):+.0f}px  "
          f"MAE {mae(agg['gap']):.0f}px")
    print(f"  predicted (cam lag) : median {med(agg['pred']):+.0f}px  "
          f"MAE {mae(agg['pred']):.0f}px")
    print(f"  recomposed gap      : median {med(agg['rec']):+.0f}px  "
          f"MAE {mae(agg['rec']):.0f}px  (near 0 => hypothesis holds)")
    # correlation gap vs predicted
    g, p = agg["gap"], agg["pred"]
    n = min(len(g), len(p))
    if n > 2:
        g, p = g[:n], p[:n]
        mg = sum(g) / n; mp = sum(p) / n
        cov = sum((g[i] - mg) * (p[i] - mp) for i in range(n))
        vg = sum((x - mg) ** 2 for x in g) ** 0.5
        vp = sum((x - mp) ** 2 for x in p) ** 0.5
        r = cov / (vg * vp) if vg > 0 and vp > 0 else float("nan")
        print(f"  corr(raw gap, predicted cam-lag gap) r = {r:+.2f}  "
              f"(near +1 => gap follows the camera)")


def _load_shots(shotdir):
    """index -> path for files like f00_frame2301.png."""
    import os, re
    out = {}
    for fn in os.listdir(shotdir):
        m = re.match(r"f(\d+)", fn)
        if m and fn.lower().endswith(".png"):
            out[int(m.group(1))] = os.path.join(shotdir, fn)
    return out


def proj_centroid_x(objs):
    """Mean projected screen x over objects (each object as a point at its T)."""
    xs = [screen_x(T_of(o[0])) for o in objs]
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else None


def cmd_track(binp, txtp, shotdir):
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import geo_overlay as go

    frames = load_frames(binp, txtp)
    nfr = len(frames)
    shots = _load_shots(shotdir)

    print(f"# fighter x-trace ({binp}) vs screenshots ({shotdir}), {nfr} frames")
    print("# per fighter side (Tx<0 = left/Akira, Tx>=0 = right/Lau); x in "
          "screen px.")
    print("# bodyX = projected body centre, shdwX = projected shadow centre, "
          "realX = screenshot shadow (green).")
    print()
    hdr = ("frm | side  bodyX  shdwX  realX | body-real  shdw-real  "
           "dBody  dShdw  dReal")
    print(hdr)
    print("-" * len(hdr))

    prev = {"L": {}, "R": {}}
    err = {"L": {"body": [], "shdw": []}, "R": {"body": [], "shdw": []}}
    frozen = {"L": [], "R": []}
    for fr in range(nfr):
        g = frames[fr]
        real = (None, None)
        if fr in shots:
            try:
                w, h, px = go.read_png(shots[fr])
                real = go.shot_shadow_centroids(px, w, h)
            except Exception:
                real = (None, None)
        for side, sel, realc in (("L", lambda t: t[0] < 0, real[0]),
                                 ("R", lambda t: t[0] >= 0, real[1])):
            b = [o for o in g["fighter"] if sel(o[0][9:12])]
            s = [o for o in g["shadow"] if sel(o[0][9:12])]
            bx = proj_centroid_x(b)
            sx = proj_centroid_x(s)
            rx = realc[0] if realc else None
            # frame-to-frame deltas (is the body matrix moving, or frozen?)
            dB = (bx - prev[side]["b"]) if bx is not None and prev[side].get("b") is not None else None
            dS = (sx - prev[side]["s"]) if sx is not None and prev[side].get("s") is not None else None
            dR = (rx - prev[side]["r"]) if rx is not None and prev[side].get("r") is not None else None
            prev[side] = {"b": bx, "s": sx, "r": rx}
            br = (bx - rx) if bx is not None and rx is not None else None
            sr = (sx - rx) if sx is not None and rx is not None else None
            if br is not None:
                err[side]["body"].append(br)
            if sr is not None:
                err[side]["shdw"].append(sr)
            if dB is not None:
                frozen[side].append(dB)

            def g6(v):
                return ("%+6.0f" % v) if v is not None else "     ."

            def g5(v):
                return ("%5.0f" % v) if v is not None else "    ."
            print(f"{fr:3d} | {side}   {g5(bx)}  {g5(sx)}  {g5(rx)} | "
                  f"{g6(br)}    {g6(sr)}   {g6(dB)} {g6(dS)} {g6(dR)}")

    def mae(xs):
        return sum(abs(x) for x in xs) / len(xs) if xs else float("nan")

    print("\nsummary -- which projection tracks the real fighter (lower MAE "
          "to realX wins):")
    for side, who in (("L", "Akira (advancing)"), ("R", "Lau (near-still)")):
        eb = err[side]["body"]; es = err[side]["shdw"]
        fz = frozen[side]
        print(f"  side {side} {who}:")
        print(f"    body-real  MAE {mae(eb):5.1f}px   shadow-real  MAE "
              f"{mae(es):5.1f}px  -> {'SHADOW' if mae(es) < mae(eb) else 'BODY'}"
              f" tracks the real fighter")
        print(f"    body frame-to-frame |dTx->px| mean {mae(fz):.1f}px "
              f"(near 0 => body matrices ~frozen)")


def _freeze_sides(objs):
    """rec -> 'L'/'R' from a reference frame, by Tx sign; drop ties (records a
    fighter shares, e.g. tiny generic primitives)."""
    tally = {}
    for mat, rec in objs:
        tally.setdefault(rec, [0, 0])
        tally[rec][0 if mat[9] < 0 else 1] += 1
    side = {}
    for rec, (nl, nr) in tally.items():
        if nl > nr:
            side[rec] = "L"
        elif nr > nl:
            side[rec] = "R"
    return side


def cmd_rtrack(binp, txtp, shotdir):
    """Record-stable body/shadow/screen trace: each object is bound to a fighter
    by its RECORD (frozen from frame 0), not by its live Tx position."""
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import geo_overlay as go

    frames = load_frames(binp, txtp)
    nfr = len(frames)
    shots = _load_shots(shotdir)
    body_side = _freeze_sides(frames[0]["fighter"])
    shad_side = _freeze_sides(frames[0]["shadow"])

    print(f"# record-stable trace ({binp}): fighter = frozen record->side from "
          f"frame 0 (ties dropped)")
    print(f"# bodies mapped: {sum(1 for v in body_side.values())} recs; "
          f"shadows mapped: {sum(1 for v in shad_side.values())} recs")
    print("# side L = Akira (screen-left), R = Lau (screen-right). x in screen "
          "px.\n")
    hdr = ("frm | side  bodyX  shdwX  realX | body-real  shdw-real  nBody")
    print(hdr)
    print("-" * len(hdr))
    err = {"L": {"b": [], "s": []}, "R": {"b": [], "s": []}}
    drift = {"L": [], "R": []}
    first = {"L": None, "R": None}
    for fr in range(nfr):
        g = frames[fr]
        real = (None, None)
        if fr in shots:
            try:
                w, h, px = go.read_png(shots[fr])
                real = go.shot_shadow_centroids(px, w, h)
            except Exception:
                real = (None, None)
        for side, realc in (("L", real[0]), ("R", real[1])):
            b = [o for o in g["fighter"] if body_side.get(o[1]) == side]
            s = [o for o in g["shadow"] if shad_side.get(o[1]) == side]
            bx = proj_centroid_x(b)
            sx = proj_centroid_x(s)
            rx = realc[0] if realc else None
            br = (bx - rx) if bx is not None and rx is not None else None
            sr = (sx - rx) if sx is not None and rx is not None else None
            if br is not None:
                err[side]["b"].append(br)
            if sr is not None:
                err[side]["s"].append(sr)
            if bx is not None:
                if first[side] is None:
                    first[side] = bx
                drift[side].append(bx - first[side])

            def g5(v):
                return ("%5.0f" % v) if v is not None else "    ."

            def g6(v):
                return ("%+6.0f" % v) if v is not None else "     ."
            print(f"{fr:3d} | {side}   {g5(bx)}  {g5(sx)}  {g5(rx)} | "
                  f"{g6(br)}    {g6(sr)}   {len(b):3d}")

    def mae(xs):
        return sum(abs(x) for x in xs) / len(xs) if xs else float("nan")

    print("\nsummary (record-stable assignment):")
    for side, who in (("L", "Akira (advancing)"), ("R", "Lau (near-still)")):
        eb, es, dr = err[side]["b"], err[side]["s"], drift[side]
        print(f"  side {side} {who}: body-real MAE {mae(eb):5.1f}px   "
              f"shadow-real MAE {mae(es):5.1f}px   "
              f"body total drift f0->f29 {dr[-1] if dr else float('nan'):+.0f}px")


def cmd_coprochk(wbin, wtxt):
    """Verify the TGP copro's RAM9 writes against the 0x0b fighter matrices in the
    snapshot: for each frame, do the copro words that land on a fighter matrix
    match the captured RAM9 value?"""
    Fs = []
    Cw = []
    for ln in open(wtxt):
        if ln.startswith("F "):
            p = ln.split()
            Fs.append((int(p[1]), float(p[3].replace(",", ".")), int(p[4], 16)))
        elif ln.startswith("C "):
            p = ln.split()
            t = float(p[1].replace(",", "."))
            Cw.append((t, (int(p[2], 16) & 0x7fff) * 4, int(p[3], 16)))
    data = open(wbin, "rb").read()
    print(f"# copro-vs-0x0b consistency ({wbin}), {len(Fs)} frames, "
          f"{len(Cw)} copro writes")
    print("frm  rsZone  figMats  matWords  coproMatch  mismatch  notWritten")
    print("-" * 64)
    for fr in range(len(Fs)):
        idx, t_end, rs = Fs[fr]
        t0 = Fs[fr - 1][1] if fr > 0 else -1.0
        raw = data[fr * FRAME_BYTES:(fr + 1) * FRAME_BYTES]
        w = list(struct.unpack("<%dI" % (FRAME_BYTES // 4), raw))
        start = (rs & 0x1ffff) >> 2
        objs, _ = walk_detailed(w, start)
        mats = {}
        for o in objs:
            if o["rec"] in EXCLUDE:
                continue
            if classify(o["mat"], o["rec"]) == "fighter" and o["mat_addr"] >= 0:
                mats[o["mat_addr"]] = True
        lastC = {}
        for (tt, addr, dv) in Cw:
            if t0 < tt <= t_end:
                lastC[addr] = dv
        tot = mm = ok = nw = 0
        for maddr in mats:
            for k in range(12):
                a = maddr + 4 + k * 4
                tot += 1
                if a in lastC:
                    if lastC[a] == w[a // 4]:
                        ok += 1
                    else:
                        mm += 1
                else:
                    nw += 1
        print(f"{fr:3d}   z{(rs & 0x1ffff)//ZONE}     {len(mats):3d}      "
              f"{tot:4d}      {ok:4d}        {mm:4d}      {nw:4d}")
    print("\n(frame 0 has no prior interval captured, so 0 matches is expected.)")
    print("coproMatch>0 with mismatch==0 => the copro writes ARE the 0x0b "
          "matrices we read; notWritten = matrices unchanged this frame "
          "(value persists in the zone).")


def _starts(txtp):
    s = {}
    for ln in open(txtp):
        p = ln.split()
        if len(p) >= 4 and p[0] == "F":
            try:
                s[int(p[1])] = int(p[3], 16)
            except ValueError:
                pass
    return s


def cmd_addrmap(binp, txtp, pose=20):
    starts = _starts(txtp)
    data = open(binp, "rb").read()
    nfr = len(data) // FRAME_BYTES

    def frame_objs(fr):
        raw = data[fr * FRAME_BYTES:(fr + 1) * FRAME_BYTES]
        w = list(struct.unpack("<%dI" % (FRAME_BYTES // 4), raw))
        start = (starts.get(fr, 0) & 0x1ffff) >> 2
        objs, jumps = walk_detailed(w, start)
        return start * 4, objs, jumps

    rs_byte, objs, jumps = frame_objs(pose)
    rs_zone = rs_byte // ZONE
    print(f"# RAM9 address map, frame {pose}  ({binp})")
    print(f"# RAM9 = 0x20000 bytes = 4 zones of 0x8000 (32 KiB). "
          f"readstart = 0x{rs_byte:05x} -> zone {rs_zone}")
    print(f"# {len(jumps)} jump(s) this frame:")
    for src, dst in jumps:
        sz, dz = src // ZONE, dst // ZONE
        tag = "  CROSS-ZONE" if sz != dz else ""
        print(f"    jump @0x{src:05x}(z{sz}) -> 0x{dst:05x}(z{dz}){tag}")
    print()
    print("grp      rec       matAddr  mz  objAddr  oz  (z=32KiB zone; "
          "* = not in readstart zone)")
    print("-" * 78)
    by_zone = {"decor": {}, "fighter": {}, "shadow": {}}
    for o in objs:
        if o["rec"] in EXCLUDE:
            continue
        g = classify(o["mat"], o["rec"])
        mz = o["mat_addr"] // ZONE if o["mat_addr"] >= 0 else -1
        oz = o["obj_addr"] // ZONE
        by_zone[g][mz] = by_zone[g].get(mz, 0) + 1
        star = "*" if mz != rs_zone else " "
        # print decor once (all share one matrix), fighters/shadows in full
        if g == "decor" and by_zone[g][mz] > 2:
            continue
        print(f"{g:8s} 0x{o['rec']:06x}  0x{o['mat_addr']:05x}  {mz}{star} "
              f"  0x{o['obj_addr']:05x}  {oz}")
    print(f"\n(decor rows truncated.) matrix-command zone histogram per group:")
    for g in ("decor", "fighter", "shadow"):
        print(f"  {g:8s}: " + ", ".join(f"zone{z}:{n}"
              for z, n in sorted(by_zone[g].items())))

    # across frames: do body matrix addresses stay put while readstart rotates?
    print("\n# across frames: readstart zone vs body/decor matrix-address span")
    print("frm  rsZone | decorMatAddr | bodyMat zones (addr range)")
    print("-" * 68)
    for fr in range(nfr):
        rsb, os_, _ = frame_objs(fr)
        rz = rsb // ZONE
        dec = [o for o in os_ if classify(o["mat"], o["rec"]) == "decor"
               and o["rec"] not in EXCLUDE]
        fig = [o for o in os_ if classify(o["mat"], o["rec"]) == "fighter"
               and o["rec"] not in EXCLUDE]
        decaddr = ("0x%05x" % dec[0]["mat_addr"]) if dec else "   .  "
        deczone = (dec[0]["mat_addr"] // ZONE) if dec else -1
        if fig:
            fa = [o["mat_addr"] for o in fig]
            fzs = sorted(set(a // ZONE for a in fa))
            frng = f"z{fzs} 0x{min(fa):05x}..0x{max(fa):05x}"
        else:
            frng = "."
        print(f"{fr:3d}    z{rz}   | {decaddr}(z{deczone}) | {frng}")


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "addrmap":
        if len(sys.argv) < 4:
            print(__doc__); sys.exit(1)
        pose = int(sys.argv[4]) if len(sys.argv) > 4 else 20
        cmd_addrmap(sys.argv[2], sys.argv[3], pose)
        return
    if len(sys.argv) >= 2 and sys.argv[1] == "coprochk":
        if len(sys.argv) < 4:
            print(__doc__); sys.exit(1)
        cmd_coprochk(sys.argv[2], sys.argv[3])
        return
    if len(sys.argv) >= 2 and sys.argv[1] == "rtrack":
        if len(sys.argv) < 5:
            print(__doc__); sys.exit(1)
        cmd_rtrack(sys.argv[2], sys.argv[3], sys.argv[4])
        return
    if len(sys.argv) >= 2 and sys.argv[1] == "track":
        if len(sys.argv) < 5:
            print(__doc__); sys.exit(1)
        cmd_track(sys.argv[2], sys.argv[3], sys.argv[4])
    elif len(sys.argv) >= 3:
        cmd_camtest(sys.argv[1], sys.argv[2])
    else:
        print(__doc__); sys.exit(1)


if __name__ == "__main__":
    main()
