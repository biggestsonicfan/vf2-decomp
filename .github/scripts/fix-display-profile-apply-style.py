from pathlib import Path
import re

p = Path("src/recovered/texture_bridge_video.c")
text = p.read_text()
pattern = re.compile(
    r'^(?P<i>[ \t]*)if \(status == VF2_OK\) (?P<s>status = [^;\n]+); \+\+exclusive;$',
    re.MULTILINE,
)
text, count = pattern.subn(
    lambda m: (
        f"{m.group('i')}if (status == VF2_OK) {{\n"
        f"{m.group('i')}    {m.group('s')};\n"
        f"{m.group('i')}}}\n"
        f"{m.group('i')}++exclusive;"
    ),
    text,
)
if count == 0:
    raise SystemExit("no misleading-indentation patterns found")
p.write_text(text)
print(f"rewrote {count} guarded accounting statements")
