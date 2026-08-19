#!/usr/bin/env python3
"""Run the full-chain validator over the tracked 0x18644 state-pair matrix."""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from game_info_18644_common import build_boundary

HERE = Path(__file__).resolve().parent
PAIR_VALIDATOR = HERE / "validate_game_info_pair.py"
DEFAULT_MATRIX = HERE.parent / "game_info_18644_state_vectors.csv"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--all", action="store_true", help="validate all 64 ordered pairs, not only recovered_exact=yes")
    parser.add_argument("--base", type=Path, help="reuse a calibrated 0x164ac snapshot")
    parser.add_argument("--workers", type=int, default=2, help="parallel ordered-pair validators")
    args = parser.parse_args()

    with args.matrix.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    pairs = [
        (int(row["state0"], 16), int(row["state1"], 16))
        for row in rows
        if args.all or row.get("recovered_exact", "").lower() == "yes"
    ]

    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        base = args.base or build_boundary(args.binary, args.rom_directory, root)

        def validate_pair(pair: tuple[int, int]) -> tuple[int, int, int, str]:
            state0, state1 = pair
            completed = subprocess.run(
                [
                    sys.executable,
                    str(PAIR_VALIDATOR),
                    str(args.binary),
                    str(args.rom_directory),
                    hex(state0),
                    hex(state1),
                    "--base",
                    str(base),
                ],
                capture_output=True,
                text=True,
            )
            output = (completed.stdout + completed.stderr).strip()
            return state0, state1, completed.returncode, output

        results = []
        with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
            futures = {pool.submit(validate_pair, pair): pair for pair in pairs}
            for future in as_completed(futures):
                results.append(future.result())

        results.sort(key=lambda item: (item[0], item[1]))
        matched = 0
        for index, (state0, state1, returncode, output) in enumerate(results, start=1):
            print(f"[{index}/{len(results)}] 0x{state0:03x}->0x{state1:03x}")
            if output:
                print(output)
            if returncode == 0:
                matched += 1
            else:
                print(f"FAILED pair 0x{state0:03x}->0x{state1:03x}", file=sys.stderr)
        print(f"matrix summary: {matched}/{len(pairs)} ordered pairs exact")
        raise SystemExit(0 if matched == len(pairs) else 1)


if __name__ == "__main__":
    main()
