# Timer interrupt path

Accepted v0.0.7 evidence for the supported VF2 Version 2.1 set.

## Runtime state

- synchronization wait: `0x0004aff8`;
- interrupt request bit: 5;
- interrupt enable value: `0x00000421`;
- i960 vector: 14;
- handler: `0x00000d50`;
- timer index: 3;
- timer reload: `0x000fffff`;
- release flag: byte at `0x0050008c`;
- wait caller return: `0x0004b07c`.

The accepted C implementation is `vf2_recovered_timer_irq_dispatch`. It is
intentionally restricted to this captured state. Other combinations of timer
requests or enable masks remain unsupported until separately traced and
compared.

## Differential result

The interpreted handler executes 33 instructions. After entering an equivalent
architectural interrupt frame on both sides, all sixteen modeled mutable memory
regions match the recovered C implementation byte for byte.
