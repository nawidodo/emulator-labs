# Coding Test — ch25: Unseen Instruction Family

You implemented conditions, the barrel shifter, data processing, load/store
and branches. Now extend your CPU with an instruction family **not covered in
the exercises**, working only from this specification (the ARM ARM "Status
register access instructions" and "Single data swap", §A4).

## Spec

### SWP / SWPB — `cond 00010 B 00 Rn Rd 0000 1001 Rm`

Atomic-style swap of a word (`B=0`) or unsigned byte (`B=1`):

1. `temp = MEM[Rn]` (byte form zero-extends)
2. `MEM[Rn] = Rm`
3. `Rd = temp`

No flags change. Modeled cost on ARM7TDMI: 1N + 2S cycles.

### MRS — `cond 00010 0 00 1111 Rd 0000 0000 0000`

`MRS Rd, CPSR` (bit22=0) or `MRS Rd, SPSR` (bit22=1): copy the whole status
register into `Rd`.

### MSR — immediate and register forms

```
MSR CPSR|SPSR_<fields>, Rm    cond 00010 R 10 mask Rd 0000 0000 0000
MSR CPSR|SPSR_<fields>, #imm  cond 00110 R 10 mask rotate   imm8
```

- `R` (bit22): 0 → CPSR, 1 → SPSR.
- `mask` bits: bit19=f → NZCV (top byte), bit16=c → control (low byte).
  Only masked fields are written; the immediate is an 8-bit value rotated
  right by `2*rotate` — same shifter rules as data processing.
- No flag side effects.

## Deliverable

Implement `exec_swap`, `exec_mrs` and `maybe_exec_status`/`apply_msr_value`
in `coding_cpu.hpp`. The hidden grader runs programs that use exactly these
encodings against the chapter runner; your register dumps must match the
reference solution's goldens bit for bit.
