# SPEC — ch38 exercise 01: ALU & shifts

Implement the R3000A ALU/shift instruction group against the register-file
abstraction in `cpu.hpp`. Three blocks:

1. `exec_alu_r` — SPECIAL three-register ops (addu subu and or xor nor slt sltu).
2. `exec_alu_i` — I-type ops (addiu slti sltiu andi ori xori lui).
3. `exec_shifts` — immediate and variable shifts.

## Hardware rules being exercised

- `$zero` writes are discarded (`Regs::set` already handles it — do not
  special-case inside the executors).
- ADDU/SUBU wrap silently; the trapping ADD/SUB forms are not in this subset.
- SLT/SLTI compare signed; SLTU/SLTIU unsigned. Note that SLTIU still
  *sign-extends its immediate* before the unsigned comparison — a classic
  MIPS gotcha pinned by `slti_sltiu_use_signed_imm`.
- Logical immediates zero-extend; arithmetic immediates sign-extend.
- LUI ignores `rs`.
- Variable shifts use `rs & 31`.

## Done when

`ch38_01_alu_tests` passes all suites on the solution build and fails RED
from the generated skeleton.
