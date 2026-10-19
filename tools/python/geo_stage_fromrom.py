"""Intruder filter for a geo-stage --from-rom export.

geo-stage --from-rom emits the captured decor (objects named ..._cap_rec...) plus the
frustum-culled decor recovered from the ROM model-ID range (..._rom_rec...). Inclusion
only catches *missing* objects; this catches the *extra* ones a blind range-fill can add:

  - LOD duplicate: a recovered object whose axis-aligned bbox overlaps strongly with a
    captured object (or another recovered one) -- probably a second level-of-detail of
    the same object.
  - Out of arena: a recovered object that falls far outside the captured decor's
    envelope.

Flagged objects are excluded by default (pass --include-flagged to keep them, tagged).
Writes <out_dir>/stage.obj + stage.mtl + stage_tex0/1.png and prints the per-stage
report: emitted / captured / recovered / flagged.

Usage: python geo_stage_fromrom.py <in_dir> <out_dir> [--include-flagged]
  <in_dir> holds the --from-rom stage.obj + stage_tex0/1.png (+ stage.mtl).
"""
import sys
import os
import glob
import shutil


def verify_mtl(out_dir):
    """Every map_Kd/map_d in the MTL must point to a file that exists. Returns the
    list of missing (material-line, referenced-path); empty = OK."""
    mtl = os.path.join(out_dir, "stage.mtl")
    missing = []
    if not os.path.exists(mtl):
        return [("(no stage.mtl)", mtl)]
    for line in open(mtl, encoding="utf-8"):
        p = line.split()
        if p and p[0] in ("map_Kd", "map_d"):
            ref = p[-1]
            if not os.path.exists(os.path.join(out_dir, ref)):
                missing.append((line.strip(), ref))
    return missing


def parse(path):
    """-> list of (name, kind, [(x,y,z)...]), kind in {'cap','rom'}."""
    verts, objs, cur = [], [], None
    for line in open(path, encoding="utf-8"):
        if line.startswith("o "):
            nm = line[2:].strip()
            kind = "rom" if "_rom_" in nm else ("cap" if "_cap_" in nm else "cap")
            cur = {"name": nm, "kind": kind, "idx": []}
            objs.append(cur)
        elif line.startswith("v "):
            p = line.split()
            verts.append((float(p[1]), float(p[2]), float(p[3])))
            if cur is not None:
                cur["idx"].append(len(verts) - 1)
    for o in objs:
        pts = [verts[i] for i in o["idx"]]
        if pts:
            o["lo"] = tuple(min(p[k] for p in pts) for k in range(3))
            o["hi"] = tuple(max(p[k] for p in pts) for k in range(3))
            o["c"] = tuple((o["lo"][k] + o["hi"][k]) / 2 for k in range(3))
        else:
            o["lo"] = o["hi"] = o["c"] = None
    return [o for o in objs if o["lo"] is not None]


def vol(lo, hi):
    return max(0.0, hi[0] - lo[0]) * max(0.0, hi[1] - lo[1]) * max(0.0, hi[2] - lo[2])


def overlap_ratio(a, b):
    """intersection volume / smaller box volume (0..1)."""
    ilo = tuple(max(a["lo"][k], b["lo"][k]) for k in range(3))
    ihi = tuple(min(a["hi"][k], b["hi"][k]) for k in range(3))
    iv = vol(ilo, ihi)
    va, vb = vol(a["lo"], a["hi"]), vol(b["lo"], b["hi"])
    m = min(va, vb)
    return iv / m if m > 1e-9 else 0.0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    include = "--include-flagged" in sys.argv
    in_dir, out_dir = args[0], args[1]
    objs = parse(os.path.join(in_dir, "stage.obj"))
    cap = [o for o in objs if o["kind"] == "cap"]
    rom = [o for o in objs if o["kind"] == "rom"]

    # captured decor envelope, expanded by a margin of 15% of its largest extent
    if cap:
        elo = tuple(min(o["lo"][k] for o in cap) for k in range(3))
        ehi = tuple(max(o["hi"][k] for o in cap) for k in range(3))
        margin = 0.15 * max(ehi[k] - elo[k] for k in range(3))
        elo = tuple(elo[k] - margin for k in range(3))
        ehi = tuple(ehi[k] + margin for k in range(3))
    else:
        elo = ehi = None

    OVL = 0.6  # strong-overlap threshold
    for o in rom:
        reason = None
        # out of arena: center outside the (expanded) captured envelope
        if elo is not None and not all(elo[k] <= o["c"][k] <= ehi[k] for k in range(3)):
            reason = "out-of-arena"
        else:
            others = cap + [r for r in rom if r is not o]
            best = max((overlap_ratio(o, b) for b in others), default=0.0)
            if best >= OVL:
                reason = "lod-dup(%.2f)" % best
        o["flag"] = reason

    flagged = [o for o in rom if o["flag"]]
    kept_rom = rom if include else [o for o in rom if not o["flag"]]

    os.makedirs(out_dir, exist_ok=True)
    # copy the MTL, the atlas sheets, and every standalone repeat-tile image
    copy = ["stage.mtl", "stage_tex0.png", "stage_tex1.png"]
    copy += [os.path.basename(p) for p in glob.glob(os.path.join(in_dir, "stage_tile*.png"))]
    for f in copy:
        src = os.path.join(in_dir, f)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(out_dir, f))
    # re-emit the obj keeping cap + kept_rom (and, if include, flagged ones tagged)
    keep_names = set(o["name"] for o in cap) | set(o["name"] for o in kept_rom)
    flag_by_name = {o["name"]: o["flag"] for o in flagged}
    with open(os.path.join(out_dir, "stage.obj"), "w", encoding="utf-8") as fo:
        fo.write("# VF2 stage (from-ROM, culled decor recovered; intruders %s). Local only.\n"
                 % ("kept+tagged" if include else "excluded"))
        emit = True
        for line in open(os.path.join(in_dir, "stage.obj"), encoding="utf-8"):
            if line.startswith("o "):
                nm = line[2:].strip()
                emit = nm in keep_names
                if emit and nm in flag_by_name:
                    fo.write("o %s_FLAG_%s\n" % (nm, flag_by_name[nm].split("(")[0]))
                    continue
            if line.startswith("mtllib"):
                fo.write("mtllib stage.mtl\n"); continue
            if emit or line.startswith(("v ", "vt ", "usemtl", "#", "mtllib")):
                fo.write(line)

    print("  emitted=%d  captured=%d  recovered=%d  flagged=%d  (kept %d)"
          % (len(cap) + len(kept_rom), len(cap), len(rom), len(flagged), len(kept_rom)))
    for o in flagged:
        print("    flag %s %s" % (o["name"], o["flag"]))

    missing = verify_mtl(out_dir)
    if missing:
        sys.stderr.write("ERROR: %s has %d MTL texture reference(s) with no file:\n"
                         % (out_dir, len(missing)))
        for ml, ref in missing:
            sys.stderr.write("  %s -> %s (MISSING)\n" % (ml, ref))
        sys.exit(2)


if __name__ == "__main__":
    main()
