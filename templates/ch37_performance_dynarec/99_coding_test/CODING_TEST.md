# Coding test — unseen rx8 extension opcodes (ch37)

The rx8 ISA grows three arithmetic extensions. Nothing else in the chapter
knows about them; YOU wire them through every tier so that the switch
interpreter, the IR pipeline, AND the optimizer all stay bit-exact.

## New opcodes (encoding per SPEC.md)

| op | mnemonic | format | semantics |
|----|----------|--------|-----------|
| 0x11 | `mul` | `rd, rs, rt` | `r[rd] = low 32 bits of r[rs] * r[rt]` (wrapping) |
| 0x12 | `not` | `rd, rs`     | `r[rd] = ~r[rs]` (bitwise complement) |
| 0x13 | `min` | `rd, rs, rt` | `r[rd] = min(r[rs], r[rt])`, SIGNED 32-bit compare |

Rules inherited from SPEC.md: r0 stays hardwired zero (writes discarded);
no faults are possible from these ops; `imm12` is ignored.

## Where to work

Implement the annotated stubs in `ext.hpp` (`@LABS` blocks 1–3):

1. **Interpreter tier** — `execute_ext()`: retire the new opcodes against a
   plain `Machine`.
2. **Translation tier** — `lower_ext()`: lower them onto extended ALU kinds
   (`AluExt::Mul/Not/Min`) riding above the base `AluOp` set.
3. **Execution tier** — the `IrEngine::ext_exec` hook plus the `run_ext()`
   driver so translated code runs identically (and survives optimization).

## Grading

The hidden manifest executes YOUR pipeline (`ch37_99_ext_runner`) on
unseen extension programs and FNV-64-hashes the observable dumps (OUT log
+ memory). Any tier disagreeing with any other tier changes the hash and
fails. The optimizer must not corrupt results either — dead-extension-op
elimination bugs count as failures, exactly as they would in production.
