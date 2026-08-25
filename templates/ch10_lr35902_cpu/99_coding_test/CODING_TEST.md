# Coding Test — ch10: Unseen Opcode Family (LDH block)

You get a written specification and an empty dispatcher. Implement the six
opcodes below in `ldh.hpp`, install `gbtest::ldh_exec` through
`cpu.extra_exec`, and make the suite (plus the hidden cases) pass.

All of them live in the high-address window `$FF00-$FFFF`:

| Opcode | Syntax | Semantics | Cycles |
|---|---|---|---|
| `E0 nn` | `ldh (n),A`   | write A to `$FF00+nn` | 12 |
| `F0 nn` | `ldh A,(n)`   | read `$FF00+nn` into A | 12 |
| `E2`    | `ldh (C),A`   | write A to `$FF00+C`  | 8  |
| `F2`    | `ldh A,(C)`   | read `$FF00+C` into A | 8  |
| `E8 dd` | `add SP,e`    | SP = SP + signed(dd)  | 16 |
| `F8 dd` | `ld HL,SP+e`  | HL = SP + signed(dd)  | 12 |

## SP-relative flag contract (both E8/F8)

- Z = 0, N = 0
- H = carry out of bit 3: `(SP & 0xF) + (dd & 0xF) > 0xF`
- C = carry out of bit 7: `(SP & 0xFF) + dd > 0xFF`

The carries are computed on the RAW unsigned byte — even when the effective
displacement is negative. This asymmetry trips almost everyone; it is why
the SM83 stack pointer arithmetic has its own opcode family instead of
reusing ADD.

## Grading

Hidden cases run your implementation over an unseen probe program and
compare a full trace against a golden hash. Visible tests only cover the
easy half on purpose.
