#!/usr/bin/env python3
"""Validate all admitted low/high masks for positive state-8 0x1645c.

Runs validate_game_info_full_dispatch.py for each of the 208 admitted masks
and reports any non-36/36 outlier. Fail-closed is preserved: unadmitted
high pairs like 0x24008140 remain 0/36 by design.
"""

import subprocess, sys, pathlib

MASKS = [
    # 4140/14140 low (16)
    "0x4140","0x4142","0x4144","0x4146","0x4150","0x4152","0x4154","0x4156",
    "0x14140","0x14142","0x14144","0x14146","0x14150","0x14152","0x14154","0x14156",
    # C140/1C140 (16)
    "0xC140","0xC142","0xC144","0xC146","0xC150","0xC152","0xC154","0xC156",
    "0x1C140","0x1C142","0x1C144","0x1C146","0x1C150","0x1C152","0x1C154","0x1C156",
    # 8140 (8) + high singles (32)
    "0x8140","0x8142","0x8144","0x8146","0x8150","0x8152","0x8154","0x8156",
    "0x4008140","0x4008142","0x4008144","0x4008146","0x4008150","0x4008152","0x4008154","0x4008156",
    "0x20008140","0x20008142","0x20008144","0x20008146","0x20008150","0x20008152","0x20008154","0x20008156",
    "0x40008140","0x40008142","0x40008144","0x40008146","0x40008150","0x40008152","0x40008154","0x40008156",
    "0x80008140","0x80008142","0x80008144","0x80008146","0x80008150","0x80008152","0x80008154","0x80008156",
    # 10140 (8) + high 26,30,31 (24)
    "0x10140","0x10142","0x10144","0x10146","0x10150","0x10152","0x10154","0x10156",
    "0x4010140","0x4010142","0x4010144","0x4010146","0x4010150","0x4010152","0x4010154","0x4010156",
    "0x40010140","0x40010142","0x40010144","0x40010146","0x40010150","0x40010152","0x40010154","0x40010156",
    "0x80010140","0x80010142","0x80010144","0x80010146","0x80010150","0x80010152","0x80010154","0x80010156",
    # 18140 (8)
    "0x18140","0x18142","0x18144","0x18146","0x18150","0x18152","0x18154","0x18156",
]

def main():
    binary = pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else pathlib.Path("build/Debug/vf2i960.exe")
    roms = pathlib.Path(sys.argv[2]) if len(sys.argv)>2 else pathlib.Path("roms/vf2")
    ok=0
    for m in MASKS:
        r=subprocess.run(["python", "decomp/i960/tools/validate_game_info_full_dispatch.py", str(binary), str(roms), "--mask", m, "--state", "8"], capture_output=True, text=True)
        line=[l for l in r.stdout.splitlines() if "summary" in l]
        s=line[-1] if line else "no summary"
        print(m, s)
        if "36/36" in s:
            ok+=1
    print(f"{ok}/{len(MASKS)} exact")
    return 0 if ok==len(MASKS) else 1

if __name__=="__main__":
    raise SystemExit(main())
