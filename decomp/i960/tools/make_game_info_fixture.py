#!/usr/bin/env python3
"""Create one reproducible 0x164ac fixture with fighter state/countdown/mode patches."""

from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

from game_info_18644_common import build_boundary, make_fixture


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom_directory", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--state0", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--state1", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--countdown", type=int, choices=(0, 1), default=0)
    parser.add_argument("--mode-bit6", type=int, choices=(0, 1), default=0)
    parser.add_argument("--base", type=Path, help="reuse an existing calibrated 0x164ac snapshot")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.base is not None:
        base = args.base.read_bytes()
    else:
        with tempfile.TemporaryDirectory() as temporary:
            base = build_boundary(args.binary, args.rom_directory, Path(temporary)).read_bytes()
    args.output.write_bytes(
        make_fixture(base, args.state0, args.state1, args.countdown, args.mode_bit6)
    )
    print(
        f"wrote {args.output}: state0=0x{args.state0:x} state1=0x{args.state1:x} "
        f"countdown={args.countdown} mode_bit6={args.mode_bit6}"
    )


if __name__ == "__main__":
    main()
