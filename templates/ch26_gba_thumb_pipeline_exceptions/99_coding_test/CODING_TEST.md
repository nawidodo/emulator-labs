# Coding Test — ch26: Fix Five Pipeline Bugs

You have a complete explicit-pipeline Thumb core (`coding_cpu.hpp`) that
builds and runs — but produces wrong traces on real programs. Five defects
are seeded behind five @LABS tasks. The hidden grader runs fixture programs
through the chapter runner and hashes the exact trace files
(`pc=<hex> op=<hex> cyc=<n> r0..r15 cpsr`), so a single wrong PC, register
or cycle count fails the case.

## What the reference semantics are

| Behavior | Reference (ARM7TDMI Thumb) |
|----------|---------------------------|
| Unconditional B target | `instr_addr + 4 + sext11(imm)*2` |
| Format-3 CMP | sets NZCV only; Rd is untouched |
| Literal load address | `instr_addr + 4 + imm*4` (imm is a WORD offset) |
| Taken branch cost | 3 cycles (2S + 1N pipeline refill) |
| Thumb BL link value | first halfword address + 4 |

## Symptoms observed on the chapter fixtures

1. Backward unconditional branches escape into high memory instead of
   looping.
2. After any `CMP Rd, #imm`, the compared register holds the difference.
3. `LDR rX, [PC, #imm]` with imm > 0 reads the wrong word.
4. Every trace's cumulative `cyc=` column drifts low once branches are
   taken.
5. BL returns land one halfword early and re-execute the second halfword.

## Deliverable

Fix all five helpers (`uncond_target`, `cmp_imm8`, `literal_addr`,
`taken_cycles`, `exec_bl_second`). Your traces must match the reference
solution's goldens byte for byte; the hidden grader also runs a program
that never appears in public material.
