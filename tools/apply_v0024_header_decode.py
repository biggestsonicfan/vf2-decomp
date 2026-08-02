from pathlib import Path
import base64
import subprocess
import zlib

WORKFLOW = Path(".github/workflows/integrate-v0024-header-decode.yml")
SCRIPT = Path("tools/apply_v0024_header_decode.py")
PATCH = "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"

subprocess.run(
    ["git", "apply", "--whitespace=error", "-"],
    input=zlib.decompress(base64.b64decode(PATCH)),
    check=True,
)

catalog = Path("decomp/i960/functions.csv")
lines = catalog.read_text(encoding="utf-8").splitlines()
if any(",texture_header_decode," in line for line in lines):
    raise SystemExit("texture_header_decode is already cataloged")
for index, line in enumerate(lines):
    if ",texture_active_prepare_call," in line:
        lines.insert(
            index + 1,
            "0x0004c180,0x0004c3f0,texture_header_decode,"
            "recovered-observed-branch,dynamic-differential+unit,"
            "Decodes the bit-packed texture header and prepares the "
            "symbol-table builder in 120 instructions; observed four times",
        )
        break
else:
    raise SystemExit("texture_active_prepare_call catalog row not found")
catalog.write_text("\n".join(lines) + "\n", encoding="utf-8")

Path("decomp/i960/notes/texture_header_decode_v0024.md").write_text(
    '# v0.0.24 texture header decode\n\n## Observed path\n\nThe helper prefix at `0x0004c180` consumes a little-endian bit stream from\n`g3`. The observed child state at `0x00550080` is zero. Eight fields are\ndecoded with the original 16-bit reservoir rules: two 8-bit dimensions, one\n8-bit code width, three 16-bit table values, one 4-bit nibble, and a final\n16-bit table value.\n\nThe dimensions are rounded with `(raw + 1) >> 1`; their product and the other\nfields are written to `0x0055c320` through `0x0055c340`. The prefix then\nprepares the register contract consumed by the already recovered symbol-table\nbuilder at `0x0004c3f0`.\n\n## Architectural fidelity\n\nThe recovery preserves the exact reservoir post-state: `r13` holds the\nremaining accumulator, `r14` the available-bit count, `r15` the next stream\naddress, `g0` the last shifted refill word, and `g11` the next 16-bit word. It\nalso reproduces all output registers and the final less comparison state.\n\nThe observed prefix is exactly 120 instructions with 36 bytes written and no\nprocedure calls or returns. It occurs four times.\n\n## Validation\n\nThe public bridge test uses the first observed header stream and verifies all\nten output values, the complete bit-reservoir post-state, instruction\naccounting, frame depth, final IP, and comparison control.\n\nThe exact VF2 2.1 ROM-backed `native-second-dispatch` validator executed 120\nreference i960 instructions for every visit and reached complete CPU and Model\n2 memory `MATCH`.\n\nThe strict totals are now 1,269,571 recovered and 1,251 interpreted\ninstructions across 180 recovered blocks and memory checkpoints, with 270 /\n300 recovered calls and returns.\n',
    encoding="utf-8",
)

WORKFLOW.unlink()
SCRIPT.unlink()
