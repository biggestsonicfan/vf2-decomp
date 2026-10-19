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
NQ = 0.05           # normal-direction quantum for the plane bucket key
DQ = 0.75           # plane-offset quantum for the coarse coplanar bucket
TIE = 0.0015        # faces within this depth are a z-fighting tie; beyond it one is in
                    # front (a lifted translucent decal, deps >= 0.003, is never a tie
                    # with its base, so it is never pushed behind -- "recule l'opaque").
GAP = 0.5           # two coplanar faces conflict when their in-plane boxes are within this
                    # gap (touching/abutting -> a seam z-fight, or overlapping). Faces far
                    # apart on the same plane (the central slab vs wall-base trim) are left
                    # alone, so the slab keeps its height instead of being sunk.


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
            cen = (sum(v[0] for v in vs) / len(vs),
                   sum(v[1] for v in vs) / len(vs),
                   sum(v[2] for v in vs) / len(vs))
            pos = n[0] * cen[0] + n[1] * cen[1] + n[2] * cen[2]   # along outward normal
            dom = max(range(3), key=lambda k: abs(n[k]))          # plane-normal axis
            ax = [k for k in range(3) if k != dom]                # the two in-plane axes
            bb = (min(v[ax[0]] for v in vs), max(v[ax[0]] for v in vs),
                  min(v[ax[1]] for v in vs), max(v[ax[1]] for v in vs))
            faces.append({"idx": idx, "obj": cur_obj, "kind": cur_kind,
                          "n": n, "d": d, "pos": pos, "bb": bb})

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

    def fronter(a, b):
        """Is object a in front of object b? Captured in front of recovered; among the
        same kind, the later-drawn (higher index) in front (MAME painter order)."""
        return (akind[a], a) > (akind[b], b)

    def overlap2d(ba, bb):   # in-plane boxes touch/overlap (gap below GAP in both axes)
        return (min(ba[1], bb[1]) - max(ba[0], bb[0]) > -GAP and
                min(ba[3], bb[3]) - max(ba[2], bb[2]) > -GAP)

    akind = {}
    for f in faces:
        akind.setdefault(f["obj"], 1 if f["kind"] == "cap" else 0)

    off = [(0.0, 0.0, 0.0)] * len(verts)
    nudged = 0
    for key, grp in buckets.items():
        # Within a coarse coplanar bucket, only faces at the SAME depth (within TIE) and
        # that actually OVERLAP in-plane z-fight. A face is pushed back one layer per
        # distinct front-er object whose face overlaps it here; the front-most overlapping
        # face stays put. A face with no front-er overlapper is never moved -- so the
        # central stone slab (no floor piece drawn later sits on top of it) keeps its
        # height, two floor tiles that merely abut (no area overlap) are left tiling, and
        # a lifted decal (a higher depth level than its wall) always stays in front.
        levels = {}
        for f in grp:
            levels.setdefault(round(f["pos"] / TIE), []).append(f)
        for lv, lgrp in levels.items():
            if len(set(f["obj"] for f in lgrp)) < 2:
                continue
            for f in lgrp:
                front_objs = set()
                for g in lgrp:
                    if g["obj"] != f["obj"] and fronter(g["obj"], f["obj"]) \
                            and overlap2d(f["bb"], g["bb"]):
                        front_objs.add(g["obj"])
                L = len(front_objs)
                if L == 0:
                    continue
                n = f["n"]                # push back along this face's own outward normal
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
