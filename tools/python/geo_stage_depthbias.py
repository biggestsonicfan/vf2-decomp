"""Coplanar depth-bias for a stage OBJ (painter-order z-fight fix).

VF2's Model 2 renderer draws decor in display-list order with exact arithmetic, so
coplanar faces never z-fight on the arcade. A baked OBJ viewed with a float z-buffer
(Blender) does: two coplanar faces shimmer in a thin zigzag along their shared area.
This happens between captured objects of one frame (seen already in the single-frame
stage_NN export) and, in the from-ROM stage_NN_full, between recovered and captured
decor that lie on the same plane.

Fix (kazuyette's rule): where faces are coplanar, the object drawn FIRST in the display
list is pushed BACK (behind the later one), mirroring MAME's painter order; and a
recovered object (_rom_) always goes behind a captured one (_cap_). The front-most face
in each coplanar group stays put; the others are nudged a hair along -normal so the
intended face wins the depth test. The geometriser emits every face with its own
vertices (no sharing), so a per-face nudge never tears the mesh.

Only coplanar groups spanning two or more DISTINCT objects are touched; a single
object's own coplanar faces are left exactly as drawn. Fighters are never run through
this (stage tool only), so character OBJs stay byte-identical.

Usage: python geo_stage_depthbias.py <stage.obj>   (rewrites in place)
       or import and call depth_bias(obj_path).
"""
import sys
import math

EPS = 0.06          # world-units nudge per painter layer (invisible, > float-z epsilon)
NEAR_PARALLEL = 0.985
PLANE_GAP = 0.6     # max distance between the two parallel planes to call them coplanar
NQ = 0.05           # normal-direction quantum for the plane bucket key
DQ = 0.75           # plane-offset quantum for the plane bucket key


def _face_normal(vs):
    a, b, c = vs[0], vs[1], vs[2]
    e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    n = (e1[1] * e2[2] - e1[2] * e2[1],
         e1[2] * e2[0] - e1[0] * e2[2],
         e1[0] * e2[1] - e1[1] * e2[0])
    ln = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
    if ln < 1e-9:
        return None
    return (n[0] / ln, n[1] / ln, n[2] / ln)


def depth_bias(path):
    lines = open(path, encoding="utf-8").read().splitlines(keepends=True)
    verts = []          # (x,y,z)
    vline = []          # index of the source line for vertex k
    faces = []          # dict: vidx list, obj index, kind, normal, plane offset d
    cur_obj = -1
    cur_kind = "cap"
    for li, ln in enumerate(lines):
        p = ln.split()
        if not p:
            continue
        if p[0] == "o":
            cur_obj += 1
            cur_kind = "rom" if "_rom_" in ln else "cap"
        elif p[0] == "v":
            verts.append((float(p[1]), float(p[2]), float(p[3])))
            vline.append(li)
        elif p[0] == "f":
            idx = [int(t.split("/")[0]) - 1 for t in p[1:]]
            vs = [verts[i] for i in idx]
            n = _face_normal(vs)
            if n is None:
                continue
            d = -(n[0] * vs[0][0] + n[1] * vs[0][1] + n[2] * vs[0][2])
            faces.append({"idx": idx, "obj": cur_obj, "kind": cur_kind,
                          "n": n, "d": d})

    # bucket faces by quantised plane (orient normal to a canonical hemisphere for the key)
    buckets = {}
    for f in faces:
        n = f["n"]
        s = 1.0
        for c in n:                      # canonical hemisphere: first non-zero component > 0
            if abs(c) > 1e-6:
                s = 1.0 if c > 0 else -1.0
                break
        key = (round(n[0] * s / NQ), round(n[1] * s / NQ), round(n[2] * s / NQ),
               round(f["d"] * s / DQ))
        buckets.setdefault(key, []).append(f)

    off = [(0.0, 0.0, 0.0)] * len(verts)
    nudged = 0
    for key, grp in buckets.items():
        objs = set(f["obj"] for f in grp)
        if len(objs) < 2:
            continue                      # single object's own coplanar faces: leave as drawn
        # front-ness per object: captured in front of recovered; later draw order in front.
        def frontness(o):
            kind = next(f["kind"] for f in grp if f["obj"] == o)
            return (1 if kind == "cap" else 0, o)
        order = sorted(objs, key=frontness, reverse=True)   # order[0] = front-most
        layer = {o: i for i, o in enumerate(order)}          # 0 = front (no nudge)
        for f in grp:
            L = layer[f["obj"]]
            if L == 0:
                continue
            n = f["n"]                    # push back along this face's own outward normal
            dx, dy, dz = -EPS * L * n[0], -EPS * L * n[1], -EPS * L * n[2]
            for i in f["idx"]:
                ox, oy, oz = off[i]
                off[i] = (ox + dx, oy + dy, oz + dz)
            nudged += 1

    # rewrite the v lines that moved
    for k, (dx, dy, dz) in enumerate(off):
        if dx == 0.0 and dy == 0.0 and dz == 0.0:
            continue
        x, y, z = verts[k]
        lines[vline[k]] = "v %.6f %.6f %.6f\n" % (x + dx, y + dy, z + dz)
    open(path, "w", encoding="utf-8").write("".join(lines))
    return nudged, len(faces)


if __name__ == "__main__":
    n, tot = depth_bias(sys.argv[1])
    print("  depth-bias: nudged %d / %d faces in coplanar multi-object groups" % (n, tot))
