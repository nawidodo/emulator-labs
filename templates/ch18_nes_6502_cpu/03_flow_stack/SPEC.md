# 03 — Flow Control: Branches, Stack Flows, Compares, INC/DEC

The ALU group is done; this exercise completes the official instruction set
with control flow — still one table row per opcode.

## Tasks

1. **Compares** — `CMP/CPX/CPY`: carry = register >= memory, N/Z from the
   discarded difference.
2. **INC/DEC** — through the provided `rmw()` helper, which models the real
   chip's dummy write of the OLD value (this is why `INC $zp` costs 5
   cycles). Register forms are trivial.
3. **Shifts** — `ASL/LSR/ROL/ROR` rotate through C; accumulator and memory
   forms.
4. **Branches** — all 8 via one helper: taken = +1 cycle, page-crossed
   target = +1 more. Not-taken stays at the 2-cycle base.
5. **Jumps/stack flows** — `JMP abs`, `JMP (ind)` with the page-wrap quirk,
   `JSR` pushing pc-1, `RTS` popping and adding 1, `RTI`, minimal `BRK`
   (full interrupt treatment in chapter 19), flag ops, `NOP`.

## Acceptance

- All tests pass, including exact branch cycle counts (2/3/4) and the
  JSR/RTS stack balance check.
