# CODING_TEST — ch39: unseen exception sequence

## The task

You are handed a CPU (`NestedCpu`, derived from the chapter's `BootMini`)
and a fully assembled BIOS program (`scenario.hpp`). The program enables
interrupts and runs two `bal`/delay-slot pairs plus one `syscall`. A fake
"interrupt controller" asserts the software-interrupt line
(`CAUSE.IP8`, bit 8 — one of the two R/W bits in CAUSE per PSX-SPX) at two
scheduled cycles:

- **IRQ1 (cycle 5)** asserts while the first `bal`'s delay-slot `nop` is
  about to execute. Expected: an Interrupt exception with **CAUSE.BD=1**,
  **EPC = 0xBFC0000C** (the branch, not the slot), vector `0xBFC00180`
  (BEV=1).
- The handler dispatches on CAUSE: `CAUSE == 0` means a plain interrupt →
  **acknowledge** (write 0 to CAUSE bits 8-9) and **retry** the preempted
  instruction; `CAUSE < 0` means BD=1 → same ack-and-retry-the-branch
  policy; otherwise it was a syscall → **skip** it (`EPC += 4`). Returns
  use `jr $t8` with **`rfe` in the jump's delay slot**.
- **IRQ2 (cycle 40)** hits the second delay slot after everything unwound.
- The mainline then stores `0xC0DE` into scratchpad word [64]
  (`0x9F800040`) and self-loops.

You implement exactly two functions in `coding_test.hpp`:

1. `deliverable()` — an interrupt may be taken iff **all** of:
   `CAUSE.IP8` set, `SR.Im` bit 8 set (the Im field occupies SR bits
   15:8), and `SR.IEc` set. Per PSX-SPX: "As long as any of the bits are
   set they will cause an interrupt if the corresponding bit is set in IM."
2. `step_irq()` — advance the cycle counter, assert IP8 on the scheduled
   cycles, and if deliverable force an Interrupt exception at the pending
   fetch address (use `raise_exception(pc, ExcCode::Interrupt)` so the
   delay-slot state produces correct BD/EPC). Otherwise execute one normal
   instruction.

## What is graded

The hidden grader runs this exercise's test binary (whole suite must pass)
and hashes this runner's trace + final-state digest after 96 cycles. Your
implementation must reproduce, deterministically:

- both interrupt entries with exact BD/EPC/vector values,
- the ack (IP8 cleared before the retry),
- the syscall skipped exactly once (EPC 0xBFC00014 → 0xBFC0018),
- final PC inside the halt self-loop with marker `0xC0DE` in scratchpad.

## Why retry-vs-skip matters

An interrupt is not a faulting instruction — nothing failed, so the
preempted instruction must simply run again. A syscall *is* the faulting
instruction, and emulating its effect means skipping it. Confusing the two
policies corrupts the stream: the tests catch both directions (a skipped
delay-slot `nop` would strand the branch target; a retried syscall would
loop forever).

## Why full nesting is out of scope here

The handler re-enables `IEc` kernel-style, but keeps interrupt depth at one:
a genuinely reentrant handler must first stack k0/k1/EPC/SR — nocash notes
plainly that the classic k0-based idiom "destroys K0", so nested contexts
need memory. Chapter 40's interrupt controller builds on this foundation.

## Reference

nocash PSX-SPX, "COP0 - Exception Handling" and "Interrupts":
<https://problemkaputt.de/psx-spx.htm#cpuexceptions>
