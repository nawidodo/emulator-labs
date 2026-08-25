# 91_challenge — boot-mini: run an exception prologue from a synthetic BIOS stub

You are given `fixtures/bios_stub.bin` (listing in `bios_stub.asm.txt`), a
hand-assembled 23-word BIOS stub that lives at the reset vector
(`0xBFC00000`, KSEG1/uncached) and installs a general exception handler at
`0xBFC00180`. The stub:

1. writes `SR = 0x00400000` via `mtc0 $t0, $12` — BEV=1 so exceptions vector
   into ROM;
2. executes `bal resume` whose **delay slot contains a `syscall`** — the
   fault fires with CAUSE.BD=1 and EPC pointing at the *branch*;
3. the handler at the BEV=1 general vector saves CAUSE/EPC into the
   scratchpad at `0x9F800000` (KSEG0 view of `0x1F800000`), adds 8 to EPC to
   skip branch + delay slot, then returns with `jr $k1` and **`rfe` in the
   jump's delay slot**;
4. resumes, round-trips the saved CAUSE through RAM, plants the completion
   marker `0x0000C0DE` in the scratchpad, and self-loops.

Your job is to complete `boot_mini.hpp` so the whole sequence runs headless.
The three `TODO` blocks are the chapter's core skills: branch/delay-slot
commit, exception entry (CAUSE/EPC/SR/vector), and COP0 moves + RFE.

## Acceptance criteria

- `ch39_91_challenge_tests` passes (unit tests pin BD/EPC semantics, the
  `jr; rfe` return sequence, scratchpad access through KSEG0, and trap
  codes).
- Running headless produces a stable trace and state digest:

```sh
./ch39_91_challenge_runner \
    --rom fixtures/bios_stub.bin --cycles 48 \
    --trace /tmp/boot_mini.log --hash-frame /tmp/boot_mini.hash
```

- The trace shows the syscall line carrying `exc=syscall bd=1
  epc=bfc0000c vec=bfc00180`.
- The final state digest matches the committed golden under
  `tests/public/ch39_ps1_exceptions_cop0_memory/` (see that directory's
  `provenance.md` for how it was generated).
- Unknown flags exit non-zero with usage on stderr; `--help` exits 0.

## Hints

- A taken branch must rewrite `next_pc_`, not `pc`; the slot instruction is
  whatever sits at `pc` when `in_delay_slot_` is true.
- On entry, EPC = branch address when (and only when) the faulting
  instruction was in that slot; the handler decides whether to retry or skip.
- RFE only pops SR shadow levels. The jump back happens because the BIOS
  idiom puts `rfe` in `jr`'s delay slot — order matters.
