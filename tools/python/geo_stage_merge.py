"""Merge several geo-stage exports of one stage into a single decor OBJ + atlas.

Each `geo-stage` run writes stage.obj / stage.mtl / stage_tex0.png / stage_tex1.png
for one capture. A decor object is posed in the camera-removed world frame, so the
same object record has identical geometry in every capture of the same stage; the
captures only differ in which records fall inside the view frustum. This unions the
object records (dedup by record, geometry taken from the first capture that has it)
and composites the per-capture colour atlases (a texel is kept from the first capture
that painted it), so the result covers as much of the stage as the inputs together do.

Usage:
    python geo_stage_merge.py <out-dir> <capture-stage-dir> [<capture-stage-dir> ...]

Each <capture-stage-dir> holds one capture's stage.obj + stage_tex0/1.png. The most
complete capture (most records) is used first. Local-only; nothing here is committed.
"""
import sys
import os
import struct
import zlib


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(path):
    d = open(path, "rb").read()
    pos, w, h, ctype, idat = 8, 0, 0, 0, b""
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        data = d[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, _bd, ctype = struct.unpack(">IIBB", data[:10])
        elif typ == b"IDAT":
            idat += data
        elif typ == b"IEND":
            break
        pos += 12 + ln
    ch = {0: 1, 2: 3, 6: 4}[ctype]
    raw = zlib.decompress(idat)
    out = bytearray(w * h * ch)
    stride = w * ch
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        for i in range(stride):
            a = line[i - ch] if i >= ch else 0
            b = prev[i]
            c = prev[i - ch] if i >= ch else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                line[i] = (line[i] + _paeth(a, b, c)) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, bytes(out), ch


def write_rgba(path, w, h, rgba):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * w * 4:(y + 1) * w * 4]
    body = (chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + body)


def parse_obj(path):
    """Return {record: [(sheet, [(vtuple, uvtuple), ...]), ...]} for each object."""
    verts, uvs, objs, cur, sheet = [], [], {}, None, 0
    for line in open(path, encoding="utf-8"):
        if line.startswith("o "):
            cur = line.rsplit("_rec", 1)[-1].strip()
            objs[cur] = []
        elif line.startswith("v "):
            verts.append(tuple(line.split()[1:4]))
        elif line.startswith("vt "):
            uvs.append(tuple(line.split()[1:3]))
        elif line.startswith("usemtl"):
            sheet = 1 if "sheet1" in line else 0
        elif line.startswith("f ") and cur is not None:
            face = []
            for tok in line.split()[1:]:
                a = tok.split("/")
                vi = int(a[0])
                ti = int(a[1]) if len(a) > 1 and a[1] else vi
                face.append((verts[vi - 1], uvs[ti - 1]))
            objs[cur].append((sheet, face))
    return objs


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    out_dir, caps = sys.argv[1], sys.argv[2:]
    os.makedirs(out_dir, exist_ok=True)
    capobjs = {c: parse_obj(os.path.join(c, "stage.obj")) for c in caps}
    order = sorted(caps, key=lambda c: -len(capobjs[c]))
    union = {}
    for c in order:
        for rec, obj in capobjs[c].items():
            union.setdefault(rec, obj)
    recs = sorted(union)

    gv, gvt, vmap, vtmap, body = [], [], {}, {}, []

    def idx(key, store, lst):
        if key in store:
            return store[key]
        lst.append(key)
        store[key] = len(lst)
        return store[key]

    for rec in recs:
        body.append("o stage_rec%s\n" % rec)
        cur = None
        for sheet, face in union[rec]:
            if sheet != cur:
                body.append("usemtl sheet%d\n" % sheet)
                cur = sheet
            body.append("f " + " ".join(
                "%d/%d" % (idx(v, vmap, gv), idx(vt, vtmap, gvt)) for v, vt in face) + "\n")
    with open(os.path.join(out_dir, "stage.obj"), "w", encoding="utf-8") as f:
        f.write("# VF2 merged stage decor: union of %d objects over %d captures. "
                "Local only, never committed.\n" % (len(recs), len(caps)))
        f.write("mtllib stage.mtl\n")
        for v in gv:
            f.write("v %s %s %s\n" % v)
        for vt in gvt:
            f.write("vt %s %s\n" % vt)
        f.write("".join(body))
    with open(os.path.join(out_dir, "stage.mtl"), "w", encoding="utf-8") as f:
        f.write("# VF2 merged stage atlas (composited over %d captures). Local only.\n" % len(caps))
        for s in (0, 1):
            f.write("newmtl sheet%d\nillum 1\nd 1.0\nmap_Kd stage_tex%d.png\nmap_d stage_tex%d.png\n"
                    % (s, s, s))

    for s in (0, 1):
        base, bw, bh = None, 0, 0
        for c in caps:
            p = os.path.join(c, "stage_tex%d.png" % s)
            if not os.path.exists(p):
                continue
            w, h, raw, ch = read_png(p)
            if base is None:
                base, bw, bh = bytearray(w * h * 4), w, h
            for i in range(w * h):
                o = i * ch
                a = raw[o + 3] if ch == 4 else 255
                if a >= 128 and base[i * 4 + 3] < 128:
                    base[i * 4:i * 4 + 3] = raw[o:o + 3]
                    base[i * 4 + 3] = 255
        if base is not None:
            write_rgba(os.path.join(out_dir, "stage_tex%d.png" % s), bw, bh, bytes(base))
    print("merged %d objects over %d captures -> %s" % (len(recs), len(caps), out_dir))


if __name__ == "__main__":
    main()
