from pathlib import Path
import base64
import subprocess
import zlib

WORKFLOW = Path(".github/workflows/integrate-v0024-game-threshold.yml")
SCRIPT = Path("tools/apply_v0024_game_threshold.py")
PATCH = "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"

subprocess.run(
    ["git", "apply", "-p1", "--whitespace=error", "-"],
    input=zlib.decompress(base64.b64decode(PATCH)),
    check=True,
)

catalog = Path("decomp/i960/functions.csv")
lines = catalog.read_text(encoding="utf-8").splitlines()
if any(",game_threshold_evaluate," in line for line in lines):
    raise SystemExit("game_threshold_evaluate is already cataloged")
for index, line in enumerate(lines):
    if ",game_color_lookup," in line:
        lines.insert(
            index + 1,
            "0x000028d4,0x00002a10,game_threshold_evaluate,"
            "recovered-observed-branch,dynamic-differential+unit,"
            "Combines two color lookups and a game-state classification into "
            "the observed below-nine threshold result; observed twice",
        )
        break
else:
    raise SystemExit("game_color_lookup catalog row not found")
catalog.write_text("\n".join(lines) + "\n", encoding="utf-8")

Path("decomp/i960/notes/game_threshold_evaluate_v0024.md").write_text(
    '# v0.0.24 game threshold evaluation\n\n'
    '## Observed path\n\n'
    'The complete function at `0x000028d4` is reached twice. It reads the '
    'current game-mode bytes, performs selector-zero and selector-one calls '
    'to the shared recovered color lookup, extracts the relevant color byte, '
    'calls the shared game-state classifier, and evaluates the observed '
    'below-nine quotient/offset threshold. Both visits return `g0 = 0` and '
    '`g1 = 0`.\n\n'
    '## Composition\n\n'
    'The wrapper contributes 38 instructions per visit and reuses the existing '
    '`game_color_lookup` and `game_state_classify` implementations. Their nested '
    'instruction, call, return and stack-write accounting is folded into the '
    'wrapper report rather than duplicated. Two wrapper checkpoints replace '
    'four standalone color-lookup and two standalone classifier checkpoints.\n\n'
    '## Validation\n\n'
    'The unit test supplies synthetic lookup tables and runtime state, verifies '
    'both nested color calls, the classifier, stack writes, final globals, '
    'instruction count, five internal calls, six returns and the complete outer '
    'procedure return.\n\n'
    'The exact VF2 2.1 ROM-backed `native-second-dispatch` executed 125 reference '
    'instructions for each visit and reached complete CPU and Model 2 memory '
    '`MATCH`.\n\n'
    'Strict totals are now 1,270,074 recovered and 748 interpreted bridge '
    'instructions, 176 recovered blocks and memory checkpoints, and 288 / 302 '
    'recovered calls and returns.\n',
    encoding="utf-8",
)

WORKFLOW.unlink()
SCRIPT.unlink()
