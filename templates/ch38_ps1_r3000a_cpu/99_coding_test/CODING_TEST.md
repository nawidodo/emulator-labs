# CODING TEST — ch38: unseen instruction family (REGIMM link variants)

You have implemented the base REGIMM encodings BLTZ/BGEZ. The hidden grading
suite exercises the **linking variants**: `BLTZAL` and `BGEZAL`.

## What you must support

Encoding: `op=0x01` (REGIMM), selecting via the `rt` field:

| rt field | mnemonic | branch when |
|----------|----------|-------------|
| 0x10 | BLTZAL rs, off | rs signed < 0  |
| 0x11 | BGEZAL rs, off | rs signed >= 0 |

Semantics that the hidden tests pin precisely:

1. `$ra = pc + 8` is written **unconditionally** — taken or not. MIPS I
   computes the link before resolving the condition; compiler output depends
   on it.
2. Displacement is relative to the delay-slot address (`pc + 4`), exactly
   like every other branch.
3. The delay slot executes once before control transfers (your ch38_03
   window machine already guarantees this).

Implement `exec_regimm_link` in `regimm_al.hpp`. The function returns false
for any encoding it does not own (including rt=0x00/0x01 plain branches).

## Grading

The hidden suite runs this exact test binary; all four suites must pass.
