# SPEC — ch38 exercise 03: branches, jumps, the delay-slot window

Five blocks in `interp.hpp`:

1. `branch_target` / `link_address` / `jump_target` — destination arithmetic.
2. `advance` — the (current_pc, next_pc, in_delay_slot) window update.
3. `exec_branch` — beq/bne/blez/bgtz/REGIMM bltz+bgez/j/jal flow decisions.
4. `exec_muldiv` — HI/LO group with documented cycle costs.
5. `cpu_step` — full fetch/dispatch/advance integration.

## The invariant under test

Across every taken and untaken branch, every jal/jr round trip:

- the delay slot executes **exactly once**,
- control reaches the target only after the slot,
- `jal` links `pc + 8` so a `jr $ra` return never re-enters the slot.

## Cycle model

1 cycle default; mult/multu cost 5; div/divu cost 37 — a documented
simplification of real non-interlocked latencies (see LECTURE.md).

## Done when

`ch38_03_branch_delay_tests` passes all suites, including the two programs
that prove slot-once and jr-through-slot behavior end to end.
