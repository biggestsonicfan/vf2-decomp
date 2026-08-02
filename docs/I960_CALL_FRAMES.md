# i960 procedure frames

VF2 relies on the i960 local-register frame mechanism. Treating every call as a
write to `g14` corrupts nested calls and produces convincing but false control
flow.

## Procedure calls

For the executor, `call` and `callx` preserve the caller's sixteen local
registers, place the return address in the saved RIP (`r2`) and create an aligned
callee frame. PFP (`r0`), SP (`r1`) and FP (`r31`) are initialized for the new
frame.

## Returns

`ret` restores the caller frame and resumes at its saved RIP. Snapshots preserve
active local frames so execution can be stopped and resumed deterministically.

## Branch-and-link

`bal` is a leaf-style link operation. `balx` additionally encodes the link
register explicitly; VF2 uses local `r14` in helpers that consume inline data
and return with `bx (r14)`.

## Validation

ROM-independent tests cover:

- two nested `call`/`ret` pairs;
- restoration of caller locals;
- stack/frame alignment;
- current and maximum frame depth;
- `balx` linking through local `r14`.
