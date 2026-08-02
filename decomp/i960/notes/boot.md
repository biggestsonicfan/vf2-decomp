# Boot path notes

## Confirmed roots

- reset entry: `0x000000b0`;
- post-IAC entry: `0x000001b0`;
- initial PRCB: `0x00003000`;
- replacement PRCB: `0x005ff410`;
- IAC packet: `0x00003860`.

## Recovered stage-one behavior

The startup path:

1. copies 56 bytes from the ROM CPU-control table at `0x00003880` to
   `0x00e00000`, stopping before the `0xffffffff` sentinel;
2. writes `0x80000000` to video control at `0x00980000`;
3. clears `0x273f8` words from work RAM base;
4. clears `0x18c00` words from work RAM offset `0x9d000`;
5. clears `0x8000` words from buffer RAM base;
6. copies `0x410` bytes from `0x00003a80` to `0x005ff000`;
7. copies `0xb0` bytes from `0x00003000` to `0x005ff410`;
8. stores the interrupt-state pointer at PRCB offset `0x14`;
9. submits an IAC `0x93` packet selecting PRCB `0x005ff410` and IP
   `0x000001b0`.

`vf2_recovered_boot_stage1_execute()` implements these effects in C. The current release
semantic executor follows 1,180,053 original i960 instructions and produces the
same CPU and mutable memory state.
