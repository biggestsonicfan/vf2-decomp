#!/usr/bin/env python3
"""Apply exact, fail-closed text edits from a JSON specification.

The spec format is:
{
  "target": "path/to/file",
  "edits": [
    {"old": "exact old text", "new": "replacement text"}
  ]
}

Each old snippet must occur exactly once at the time its edit is applied.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("spec", type=Path)
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    target = Path(spec["target"])
    text = target.read_text(encoding="utf-8")

    for index, edit in enumerate(spec["edits"], start=1):
        old = edit["old"]
        new = edit["new"]
        count = text.count(old)
        if count != 1:
            raise SystemExit(
                f"edit {index}: expected old snippet exactly once in {target}, found {count}"
            )
        text = text.replace(old, new, 1)

    target.write_text(text, encoding="utf-8")
    print(f"applied {len(spec['edits'])} exact edits to {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
