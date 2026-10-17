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

Usage:  geo_camtest.py <ram9.bin> <readstarts.txt>

Pure stdlib. Convention: a matrix is 12 floats m[0..11] applied as MAME
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


def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    binp, txtp = sys.argv[1], sys.argv[2]
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


if __name__ == "__main__":
    main()
