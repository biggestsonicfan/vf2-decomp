# v0899 — Original frame-342 AD 21 00 descriptor allocation

Date: 2026-10-20. Recovered and tested against the owner's original VF2.1
MC68000 audio ROM **without committing ROM bytes, sound buffers or snapshots**.

## Original ROM branch evidence

The last command in the authentic i960 frame-342 UART stream was
`AD 21 00`. Both the original MC68000 L3 entry and native C already
agreed on its **15-byte** UART transport and five resulting ring files
(see v0897). Native C previously refused this particular A0 packet
at `0x1E98`, even though the original 68000 completed the dispatcher.

The ROM tables explain the route:

| ROM/source item | Original value | Consequence |
| --- | --- | --- |
| `0x608E12 + 0x21*4` | `0x6091A8` | AD payload selects row |
| Word at `0x6091A8` | `0x007B` | Third byte `0x00` passes signed max gate |
| Longword at `0x6091AA` | `0x60A6B2` | Record for third-byte index zero |
| Byte at `0x60A6B2` | `0xCF` | Bit 7 branches into descriptor search `0x1ED4` |

The same bounded original `0x1ED4` first-vacant-descriptor path
already recovered for `AD 23` and `AD 24` applies to **exactly**
`AD 21 00` here. Its original code clears the first available
32-byte descriptor and installs the original `record + 1` pointer,
word `1` and high activation bit before returning through the
normal dispatcher epilogue.

The C implementation adds only the exact `AD 21 00` selector
under the existing bit-7, mapped ROM-pointer and bounded first-vacant
checks. No broader AD21 range or all-busy seventh-slot semantics
were enabled.

## Executed original-ROM differential

The actual Linux GCC GitHub Actions sound-differential executable
for commit `6c889b83`, from workflow
[CI #2227](https://github.com/celsowm/vf2-decomp/actions/runs/39362776444),
was executed with the **original owner ROM and sample files**,
as `vf2_m68k_frame342_dispatch_tests /path/to/private/vf2-roms`.
Exit code: **0**.

```text
FRAME342_DISPATCH index=4 entry=ad2100 class=a0 C_status=0 C_handler=01f02 C_handled=1 S_status=0 S_pc=0265c S_steps=248 SCSP_active_before=0 SCSP_active_C=0 SCSP_active_S=0 first_ram_diff=none
FRAME342_AD21_C68000_PARITY differences=0 first=80000 C_handler=01f02 S_steps=248
```

- Original CPU: **248** interpreter instructions through dispatch exit
  at `0x265C`.
- Native C: handler `0x1F02`, packet retired once.
- Whole 512 KiB modeled sound RAM comparison **zero differences**
  excluding the original interpreter's stack scratch
  (`0x7E00..0x7FFF`, intentionally not modeled by C).
- Modeled SCSP: zero new active slots on either side. This packet
  installs a descriptor; it does **not** itself generate PCM/KEYON.
- The other four frame-342 commands continued to pass their
  independently recovered dispatch and original-SCSP differential;
  `9E 0A 7F` retains **4096/4096** stereo sample model parity
  and nonzero PCM output (v0898).

An independent **ROM-free** CTest, `vf2_m68k_ad21_frame342_rom_free`,
covers a synthetic ROM-shaped AD21 selector, a refused neighboring
third byte (`AD 21 01`) and a header with bit 7 cleared.
The check is intentionally separate from private owner-ROM tests.

## Next frontier and constraints

The meaningful next step is **native descriptor stream progression**
after the authentic `AD 21 00`, under the original ROM's stream
maintenance/timing rules. Do not call AD21 an audible effect:
the verified packet files a descriptor but activates no SCSP channel
at this dispatch checkpoint. Also do not infer a fighter collision
from the KICK controller bit in the previously observed i960 frame.

The currently verified i960 -> vector-15 -> UART -> L3 -> isolated
MC68000 command dispatch chain is **not yet** a cycle-accurate
simultaneous dual-CPU sound system.
