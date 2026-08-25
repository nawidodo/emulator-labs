# 01 — Interrupt Flows: RESET, BRK, IRQ, NMI

The ch18 core executed instructions; a real NES CPU also has four ways to
leave the instruction stream. This exercise wires them in with the details
that separate a working CPU from an accurate one.

## Tasks

1. **BRK** — fetch the padding byte (BRK is 2 bytes: pushed PC = BRK address
   + 2), push PCH/PCL/P **with the B bit set**, raise I, vector $FFFE/$FFFF.
2. **RESET** — S -= 3, I set, PC from $FFFC/$FFFD; official billing is 7
   cycles.
3. **IRQ/NMI sequences** — one shared implementation: two dummy opcode
   reads, push PCH/PCL/P with **B CLEAR** (the ONLY stacked-P difference
   from BRK), raise I, then read the vector. NMI reads $FFFA and ignores I;
   IRQ reads $FFFE. Each sequence costs exactly 7 cycles of real accesses.
4. **Polling** — `step()` polls before fetching the next opcode: an
   edge-latched NMI request (`set_nmi_line`) is serviced exactly once per
   quiet->high transition regardless of I; `irq_line` is level-sensitive and
   honored only while I is clear.

## Acceptance

- All `interrupts.*` tests pass: B-bit stacking, vector selection, I-flag
  masking, edge-latch semantics, exact 7-cycle interrupt cost.
