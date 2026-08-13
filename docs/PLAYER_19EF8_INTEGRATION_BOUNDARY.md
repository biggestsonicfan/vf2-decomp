# Player `0x19ef8` integration boundary

The selector-setup language interpreted by `0x1a1e4` is now recovered independently from the outer `0x19ef8` caller.

This distinction is intentional:

- selector bytecode support is a property of the generic `0x1a1e4` interpreter;
- `0x19ef8` still owns separate pre/post-selector control flow and later `0x26ef0` / `0x27130` behavior;
- therefore a selector that parses successfully must not automatically be classified as a fully recovered `0x19ef8` path.

The integration sequence is:

1. replace the manual selector-`0x505` setup block in `hybrid_execute_player_19ef8()` with the semantic `0x1a1e4` interpreter while retaining the caller's existing `0x505` guard;
2. require the same V2.2 differential endpoint;
3. remove the selector-value guard only after the caller's remaining state predicates are expressed semantically; and
4. keep `0x26ef0` / `0x27130` as explicit independent recovery boundaries rather than adding selector-specific exceptions.

This prevents the old anti-pattern of treating a caller-state limitation as an unsupported selector or fighter family.
