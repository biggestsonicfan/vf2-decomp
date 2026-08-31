#!/usr/bin/env python3
"""
Targeted dynamic taint for the i960 corridor (AGENTS.md next work #3).

Tracks data flow for the operations actually seen in the target corridor:
  loads (ld/ldob/ldis/ldl etc), moves (mov/movl/movt/movq),
  bitwise (and/andnot/notand/or/ornot/notor/xor/xnor/nor/nand/clrbit/setbit/alterbit/notbit),
  shifts (shlo/shli/shro/shri/rotate/shrdi), add/sub (addo/addi/subo/subi),
  compares (cmpo/cmpi/cmpor/cmpr etc), and conditional branches (bbc/bbs/cmpi* branches/be/bne/bl...).

Output is sideband evidence such as:
  branch 0x00018698 depends on:
    fighter0 + 0x1a4 bit 6
    fighter0 + 0x5b6

Metadata is kept outside architectural CPU state (never influences oracle).
Usage:
  python tools/python/taint.py --rom-dir roms/vf2 --scenario out/state8-positive.json --trace out/state8-case-mnem.jsonl
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import defaultdict, Counter
from pathlib import Path
from typing import Dict, Set, Tuple, List

DEFAULT_WINDOW = 0x2000

def parse_int(v):
    if isinstance(v, int):
        return v
    return int(str(v), 0)

def load_scenario_bases(scenario_path: Path) -> Dict[str,int]:
    data = json.loads(scenario_path.read_text(encoding="utf-8"))
    meta = data.get("metadata", {})
    bases = {}
    for k in ("fighter0","fighter1"):
        if k in meta:
            bases[k] = parse_int(meta[k])
    # also allow explicit --base overrides
    return bases, data

def disasm_ips(rom_dir: Path, vf2i960: Path, ips: Set[int]) -> Dict[int, Dict]:
    """Disassemble each executed IP individually (avoids bulk decode failures on data)."""
    mp: Dict[int, Dict] = {}
    for ip in sorted(ips):
        proc = subprocess.run([str(vf2i960), "disasm", str(rom_dir), hex(ip), "1"],
                              text=True, capture_output=True)
        # even if returncode !=0, stdout may contain the one valid line; only use stdout
        for line in proc.stdout.splitlines():
            line=line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            mnemonic = parts[2]
            ops = " ".join(parts[3:]) if len(parts) > 3 else ""
            mp[addr] = {"mnemonic": mnemonic, "ops": ops, "raw": line}
    return mp

# quick helpers to parse operands for taint
REG_RE = re.compile(r'\b(r\d+|g\d+|fp)\b')
MEM_OFFSET_RE = re.compile(r'0x([0-9a-fA-F]+)\((r\d+|g\d+|fp)\)')
IMM_RE = re.compile(r'^\s*(0x[0-9a-fA-F]+|\d+)\s*$')

def regs_in_ops(ops: str) -> List[str]:
    return REG_RE.findall(ops)

def parse_mem_operands(ops: str) -> List[Tuple[int,str]]:
    """return list of (offset, base_reg) for patterns 0x... (reg)"""
    res=[]
    for m in MEM_OFFSET_RE.finditer(ops):
        off = int(m.group(1),16)
        base = m.group(2)
        res.append((off, base))
    return res

def tag_for_address(addr: int, bases: Dict[str,int], window: int) -> str | None:
    for name, base in bases.items():
        off = addr - base
        if 0 <= off < window:
            return f"{name} + 0x{off:04x}"
    return None

# instruction classes
LOAD_MN = {"ld","ldob","ldib","ldos","ldis","ldl","ldt","ldq","lda"}
STORE_MN = {"st","stob","stib","stos","stis","stl","stt","stq","stis"}
MOVE_MN = {"mov","movl","movt","movq"}
BITWISE_MN = {"and","andnot","notand","or","ornot","notor","xor","xnor","nor","nand","clrbit","setbit","alterbit","notbit","not"}
SHIFT_MN = {"shlo","shli","shro","shri","rotate","shrdi"}
ADD_SUB_MN = {"addo","addi","subo","subi","addc","subc","addr","subr","mulr","divr","mulo","muli","remo","remi","modi","divo","divi"}
COMPARE_MN = {"cmpo","cmpi","cmpor","cmpr","cvtri","cvtzri","cvtir","cmpdeco","cmpdeci","cmpinco","cmpinci","chkbit","scanbit","spanbit","concmpo","concmpi"}
BRANCH_MN_PREFIX = ("bbc","bbs","cmpi","cmpo","be","bne","bl","ble","bg","bge","bo","bno","cmpib","cmpobe","cmpobne","cmpobe","cmpobge","b")  # fallback
COND_BRANCH = {"bbc","bbs","be","bne","bl","ble","bg","bge","bo","bno","b"}

def is_branch(mn: str) -> bool:
    return mn in COND_BRANCH or mn.startswith("bbc") or mn.startswith("bbs") or mn.startswith("cmpi") or mn.startswith("cmpo") or mn.startswith("cmpob") or mn in {"be","bne"}

def run_taint(trace_path: Path, rom_dir: Path, vf2i960: Path, bases: Dict[str,int], window: int, disasm_map: Dict[int,Dict]) -> Dict:
    # pending memory accesses per step
    pending: Dict[int, List[Dict]] = defaultdict(list)
    steps = []
    finals = []
    # first pass collect records
    with trace_path.open("r", encoding="utf-8") as f:
        for line in f:
            line=line.strip()
            if not line:
                continue
            rec=json.loads(line)
            t=rec.get("type")
            if t=="memory":
                pending[parse_int(rec["step"])].append(rec)
            elif t=="step":
                steps.append(rec)
            elif t=="final":
                finals.append(rec)
    steps.sort(key=lambda r: parse_int(r["step"]))
    # taint state: reg -> set(tag)
    taint: Dict[str, Set[str]] = defaultdict(set)
    # g regs are aliases: g0 = r16 etc. For simplicity keep both names but propagate together?
    # We'll normalize: gN -> r(16+N)
    def norm_reg(r: str) -> str:
        if r.startswith("g"):
            try:
                n=int(r[1:])
                return f"r{16+n}"
            except: return r
        return r

    condition_taint: Set[str] = set()
    branch_deps: Dict[int, Set[str]] = {}
    # also track per-IP execution count
    ip_counts = Counter()

    for rec in steps:
        step = parse_int(rec["step"])
        ip_before = parse_int(rec["ip_before"])
        ip_after = parse_int(rec.get("ip_after", ip_before))
        mnemonic = rec.get("mnemonic") or disasm_map.get(ip_before, {}).get("mnemonic", "")
        ops = disasm_map.get(ip_before, {}).get("ops", "")
        ip_counts[ip_before]+=1

        # fetch memory accesses for this step
        mems = pending.pop(step, [])
        # tag memory reads that fall in fighter window
        mem_tags = []
        for m in mems:
            addr = parse_int(m.get("address",0))
            kind = m.get("kind","read")
            tag = tag_for_address(addr, bases, window)
            if tag and kind=="read":
                # include size? keep simple
                mem_tags.append(tag)
            elif tag and kind=="write":
                # writes are sinks, but we note tag for future loads? For taint, writes don't create new taint
                pass

        # apply taint transfer
        # determine dest registers (heuristic: last reg operand is dest for many)
        regs = regs_in_ops(ops)
        # normalize
        regs_norm = [norm_reg(r) for r in regs]

        # Helper to get taint of src regs
        def union_of(regs_list: List[str]) -> Set[str]:
            s=set()
            for r in regs_list:
                s.update(taint.get(norm_reg(r), set()))
            return s

        # classify
        mn = mnemonic

        if mn in LOAD_MN:
            # ld  offset(base), dst : taint dst = union(mem_tags) ∪ taint(base) if base tainted? plus immediate offset not
            # For fighter loads, the address base being fighter pointer also matters, but fighter pointer itself is base
            # So if mem_tag exists, that's the source; otherwise fallback to base reg taint
            if regs_norm:
                dst = regs_norm[-1]
                src_taint = set(mem_tags)
                # also include base reg taint if mem_tags empty and base reg has tag (pointer derived from fighter)
                # try to include base reg from mem operand
                mem_ops = parse_mem_operands(ops)
                for off, base in mem_ops:
                    src_taint.update(taint.get(norm_reg(base), set()))
                    # if base is fighter pointer and offset is fighter field, we already have tag; else keep base tag
                taint[dst] = src_taint
        elif mn in STORE_MN:
            # store does not create new reg taint, but memory write could be considered? ignore for now
            pass
        elif mn in MOVE_MN:
            if len(regs_norm) >=2:
                dst = regs_norm[-1]
                src = regs_norm[0]
                # for movt/movl etc with 3-4 regs, we need to handle multiple? Simplify: dst regs = last N, src = first N
                # If mnemonic movl/movt/movq uses multiple regs, copy each pair. Our regs_in_ops returns all, but we treat as pairwise?
                # For simplicity, if count >2 and mnemonic in movl/movt/movq, map src->dst pairs
                if mn in {"movl","movt","movq"} and len(regs_norm)>2:
                    # determine count: movl 2 regs, movt 3, movq 4. We'll pair
                    cnt = 2 if mn=="movl" else 3 if mn=="movt" else 4
                    # assume regs are interleaved src then dst? example movl rX, rY with 2 regs each not clear. Keep simple union
                    taint[dst] = union_of(regs_norm[:-1])
                else:
                    taint[dst] = set(taint.get(src, set()))
            elif len(regs_norm)==1:
                # mov 0, r10 immediate -> clear taint
                dst = regs_norm[0]
                # immediate has no taint
                # if ops contains immediate, clear
                if IMM_RE.search(ops.split(",")[0] if "," in ops else ops):
                    taint[dst]=set()
                else:
                    # unknown
                    taint[dst]=set()
        elif mn in BITWISE_MN or mn in SHIFT_MN or mn in ADD_SUB_MN:
            if regs_norm:
                dst = regs_norm[-1]
                srcs = regs_norm[:-1]
                # also immediate not tainted
                combined=set()
                for s in srcs:
                    combined.update(taint.get(s,set()))
                # for shifts with immediate count, immediate not tainted
                taint[dst]=combined
        elif mn in COMPARE_MN:
            # compare sets condition flags, not a dest reg, but updates condition_taint
            src_taint=set()
            for r in regs_norm:
                src_taint.update(taint.get(r,set()))
            # also include mem_tags if compare involves memory load? But compare operands are regs usually after load
            src_taint.update(mem_tags)
            condition_taint = src_taint
            # some compare also write dest? e.g., chkbit sets condition only, not reg. So no reg update.
            # scanbit writes dest reg
            if mn in {"scanbit","spanbit"} and regs_norm:
                dst = regs_norm[-1]
                taint[dst]=set(src_taint)
        elif is_branch(mn):
            # branch depends on condition_taint plus direct operand regs (bbc/bbs)
            deps=set(condition_taint)
            # for bbc/bbs, first operand is bit number (immediate) not tainted, second is reg
            if mn in {"bbc","bbs"} and len(regs_norm)>=1:
                # last reg is tested
                deps.update(taint.get(regs_norm[-1], set()))
                # also note bit number: if branch is bbc 4,r7 etc, bit 4 matters - annotate tag with bit?
                # we can refine tags that are fighter+... to include bit info
                # For output, append " bit X" if tag contains fighter and branch is bit test
                bit_text=""
                # extract bit number from ops: first number
                m=re.search(r'^\s*(\d+)', ops)
                if m:
                    bit_text = f" bit {m.group(1)}"
                    # annotate deps that are fighter tags
                    annotated=set()
                    for d in deps:
                        if "fighter" in d and "bit" not in d:
                            annotated.add(f"{d}{bit_text}")
                        else:
                            annotated.add(d)
                    deps=annotated
            elif len(regs_norm)>=1 and mn not in COND_BRANCH:
                # other direct branches like be/bne have no reg operands, rely on condition_taint
                pass
            else:
                # for branches that are direct compares like cmpibg etc, they are actually compare+branch fused?
                # Those are not separate condition, they compare and branch directly: e.g., cmpibg r9,r3, target
                # Then deps = taint of both regs
                for r in regs_norm:
                    deps.update(taint.get(r,set()))
            branch_deps[ip_before]=deps
            # after branch, condition_taint may be consumed but keep
        else:
            # unknown mnemonic (call, bal, lda, etc.) -> for call, clear condition? keep
            if mn in {"call","callx","bal","balx","bx"}:
                # call may clobber some regs but we keep conservative: don't clear
                pass
            elif mn=="lda":
                # lda is address calc: dst gets taint of base reg
                if regs_norm:
                    dst = regs_norm[-1]
                    srcs = regs_norm[:-1]
                    taint[dst]=union_of(srcs)
            else:
                # unhandled: propagate conservatively? For safety, if dst reg exists, union of srcs
                if regs_norm and "," in ops:
                    # heuristic: last reg is dest
                    dst = regs_norm[-1]
                    srcs = regs_norm[:-1]
                    if srcs:
                        taint[dst]=union_of(srcs)

        # also handle case where instruction had memory read but not classified as load (e.g., cmp with memory) - already via mem_tags added to condition?

    # any remaining pending memory not attributed (should be none)
    return {"branch_deps": branch_deps, "ip_counts": ip_counts, "finals": finals, "bases": bases}

def main():
    ap=argparse.ArgumentParser(description="Targeted taint for i960 fighter corridor")
    ap.add_argument("--rom-dir", required=True, help="ROM directory")
    ap.add_argument("--vf2i960", default="build/Debug/vf2i960.exe", help="path to vf2i960")
    ap.add_argument("--scenario", required=True, help="scenario JSON with fighter bases")
    ap.add_argument("--trace", required=True, help="vf2probe trace JSONL with --trace and --memory-trace and mnemonic")
    ap.add_argument("--window", type=lambda x: int(x,0), default=DEFAULT_WINDOW)
    ap.add_argument("--json", help="output JSON")
    ap.add_argument("--until", type=lambda x: int(x,0), help="optional until filter for branch IPs")
    args=ap.parse_args()

    bases, scenario_data = load_scenario_bases(Path(args.scenario))
    if not bases:
        raise SystemExit("no fighter bases in scenario metadata")

    # build disasm cache around fighter corridor (0x164ac..0x18fff) plus any IPs seen in trace
    # first collect IPs from trace to know range
    ips=set()
    with Path(args.trace).open() as f:
        for line in f:
            try:
                rec=json.loads(line)
                if rec.get("type")=="step":
                    ips.add(parse_int(rec["ip_before"]))
            except: pass
    if not ips:
        raise SystemExit("no steps in trace")
    disasm_map = disasm_ips(Path(args.rom_dir), Path(args.vf2i960), ips)

    result = run_taint(Path(args.trace), Path(args.rom_dir), Path(args.vf2i960), bases, args.window, disasm_map)

    # print human report
    branch_deps = result["branch_deps"]
    for ip, deps in sorted(branch_deps.items()):
        if args.until is not None and ip != args.until:
            continue
        print(f"branch 0x{ip:08x} depends on:")
        if not deps:
            print("  (no fighter taint - condition clear or immediate)")
        else:
            for d in sorted(deps):
                print(f"  {d}")
        # also show mnemonic if available
        mn = disasm_map.get(ip, {}).get("mnemonic","?")
        print(f"  ; {mn} {disasm_map.get(ip,{}).get('ops','')}")

    if args.json:
        out = {
            "bases": bases,
            "branch_deps": {f"0x{ip:08x}": sorted(list(deps)) for ip,deps in branch_deps.items()},
            "trace": str(args.trace),
            "window": args.window,
        }
        Path(args.json).write_text(json.dumps(out, indent=2, sort_keys=True)+"\n", encoding="utf-8")
        print(f"wrote {args.json}", flush=True)

if __name__=="__main__":
    main()
