#!/usr/bin/env python3
"""Phase 3 coverage report: how much of the program a session executes, and
which executed code is not yet a recovered function.

Inputs
------
- ``decomp/i960/functions.csv`` -- the recovered-function registry
  (address, end, name, status, ...).
- One or more session coverage files produced by
  ``vf2i960 coverage <roms> <snap> <frames> <out.json> [script]``:
  compact JSON mapping each executed program word to a hit count.

For each session and for their union the tool reports, as percentages a reader
can re-derive:

- ``recovered``   -- executed words that fall inside a recovered function;
- ``unrecovered`` -- executed words that do not (the Phase 3 backlog);
- ``lifted``      -- always 0 here (the build-time recompiler is the G1/Phase 4
                     decision; this tool has a column so the number is explicit);
- never-executed recovered functions (0 executed words across the union).

``interpreted`` versus ``recovered`` for the *shipped* hybrid runtime is a
different axis, reported per run by ``vf2_host`` (the ``interp`` status line);
this tool measures the i960 program itself, from the interpreter oracle, so its
split is recovered-vs-unrecovered code regardless of which engine runs it.

The ranked backlog (contiguous runs of executed-but-unrecovered words, by hit
count) is written with ``--backlog-out``; it holds addresses and counts only
and is safe to commit.

Usage
-----
    python tools/python/coverage_report.py \
        --session attract=out/cov_attract.json \
        [--session coin=out/cov_coin.json ...] \
        [--functions decomp/i960/functions.csv] \
        [--backlog-out decomp/i960/coverage_backlog.csv]
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class Function:
    start: int
    end: int
    name: str
    status: str

    @property
    def words(self) -> int:
        return max(0, (self.end - self.start) // 4)


def parse_int(text: str) -> int:
    return int(text, 16) if text.lower().startswith("0x") else int(text)


def load_functions(path: Path) -> List[Function]:
    funcs: List[Function] = []
    with path.open(newline="") as handle:
        for row in csv.DictReader(handle):
            addr, end = row.get("address", ""), row.get("end", "")
            if not addr.startswith("0x") or not end.startswith("0x"):
                continue
            funcs.append(
                Function(parse_int(addr), parse_int(end),
                         row.get("name", ""), row.get("status", ""))
            )
    funcs.sort(key=lambda f: f.start)
    return funcs


class FunctionIndex:
    """Half-open [start, end) interval lookup by address."""

    def __init__(self, funcs: List[Function]) -> None:
        self.funcs = funcs
        self.starts = [f.start for f in funcs]

    def find(self, addr: int) -> Optional[Function]:
        i = bisect.bisect_right(self.starts, addr) - 1
        if i < 0:
            return None
        f = self.funcs[i]
        return f if f.start <= addr < f.end else None


def load_session(path: Path) -> Dict[int, int]:
    data = json.loads(path.read_text())
    return {parse_int(k): int(v) for k, v in data["counts"].items()}


def group_runs(addresses: List[int], hits: Dict[int, int]) -> List[Tuple[int, int, int, int]]:
    """Contiguous 4-byte runs -> (start, end, words, total_hits)."""
    runs: List[Tuple[int, int, int, int]] = []
    for addr in sorted(addresses):
        if runs and addr == runs[-1][1]:
            s, _, w, h = runs[-1]
            runs[-1] = (s, addr + 4, w + 1, h + hits[addr])
        else:
            runs.append((addr, addr + 4, 1, hits[addr]))
    return runs


def summarize(name: str, counts: Dict[int, int], index: FunctionIndex) -> dict:
    recovered = [a for a in counts if index.find(a) is not None]
    unrecovered = [a for a in counts if index.find(a) is None]
    rec_hits = sum(counts[a] for a in recovered)
    unrec_hits = sum(counts[a] for a in unrecovered)
    return {
        "name": name,
        "distinct": len(counts),
        "recovered_words": len(recovered),
        "unrecovered_words": len(unrecovered),
        "total_hits": rec_hits + unrec_hits,
        "recovered_hits": rec_hits,
        "unrecovered_hits": unrec_hits,
        "unrecovered_addrs": unrecovered,
    }


def pct(part: int, whole: int) -> str:
    return f"{(100.0 * part / whole):6.2f}%" if whole else "   n/a"


def print_summary(summ: dict) -> None:
    d = summ["distinct"]
    h = summ["total_hits"]
    print(f"  {summ['name']:<16} "
          f"words={d:<7} "
          f"recovered={pct(summ['recovered_words'], d)} "
          f"unrecovered={pct(summ['unrecovered_words'], d)} "
          f"lifted={pct(0, d)}")
    print(f"  {'':<16} "
          f"hits={h:<9} "
          f"recovered={pct(summ['recovered_hits'], h)} "
          f"unrecovered={pct(summ['unrecovered_hits'], h)}")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--functions", type=Path,
                        default=Path("decomp/i960/functions.csv"))
    parser.add_argument("--session", action="append", default=[],
                        metavar="NAME=PATH",
                        help="a coverage JSON file, labelled (repeatable)")
    parser.add_argument("--backlog-out", type=Path, default=None,
                        help="write the ranked unrecovered-executed backlog here (CSV)")
    args = parser.parse_args(argv)

    if not args.session:
        parser.error("at least one --session NAME=PATH is required")

    funcs = load_functions(args.functions)
    index = FunctionIndex(funcs)
    total_func_words = sum(f.words for f in funcs)
    print(f"functions.csv: {len(funcs)} recovered functions, "
          f"{total_func_words} words")

    sessions: List[Tuple[str, Dict[int, int]]] = []
    for item in args.session:
        if "=" not in item:
            parser.error(f"--session expects NAME=PATH, got {item!r}")
        name, path = item.split("=", 1)
        sessions.append((name, load_session(Path(path))))

    union: Dict[int, int] = {}
    print("\nper-session executed-code coverage")
    for name, counts in sessions:
        print_summary(summarize(name, counts, index))
        for addr, n in counts.items():
            union[addr] = union.get(addr, 0) + n

    union_summ = summarize("UNION", union, index)
    print("\nunion")
    print_summary(union_summ)

    # Recovered functions never executed in the union.
    executed_funcs = {id(index.find(a)) for a in union if index.find(a)}
    never = [f for f in funcs if id(f) not in executed_funcs]
    print(f"\nrecovered functions exercised: {len(funcs) - len(never)}/{len(funcs)}"
          f"  (never-executed: {len(never)})")
    func_pct = 100.0 * (len(funcs) - len(never)) / len(funcs) if funcs else 0.0
    print(f"recovered-function coverage: {func_pct:.2f}% of recovered functions"
          f" touched by these sessions")

    # Ranked backlog: executed words with no recovered function, grouped.
    runs = group_runs(union_summ["unrecovered_addrs"], union)
    runs.sort(key=lambda r: r[3], reverse=True)
    print(f"\nbacklog (executed, not yet recovered): "
          f"{union_summ['unrecovered_words']} words in {len(runs)} runs")
    for start, end, words, hits in runs[:20]:
        print(f"  0x{start:08x}-0x{end:08x}  words={words:<5} hits={hits}")
    if not runs:
        print("  (none: every executed word is inside a recovered function)")

    if args.backlog_out is not None:
        with args.backlog_out.open("w", newline="") as handle:
            writer = csv.writer(handle)
            writer.writerow(["start", "end", "words", "hits"])
            for start, end, words, hits in runs:
                writer.writerow([f"0x{start:08x}", f"0x{end:08x}", words, hits])
        print(f"\nbacklog written to {args.backlog_out}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
