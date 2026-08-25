# Chapter 13 — Timers and Interrupts

The SM83 has one clock, one timer, and five interrupt lines. This chapter
builds all three layers exactly and deterministically, on top of the
chapter-11 CPU interface (copied self-contained into `01_divider/`, trimmed
to fetch/step/hooks + ALU/loads/jumps; the IntCtl/IrqHook/IntBus interrupt
machinery is the same model ch11 taught — here it is infrastructure).

## The divider: one counter, two registers

There is no "DIV register" and no "TIMA prescaler" as separate pieces of
silicon. There is ONE free-running 16-bit internal counter that gains one
count per T-cycle (the model advances it in whole 4-T-cycle blocks). What
games see:

* **DIV ($FF04)** is the HIGH byte of that counter. Reading costs nothing;
  it changes value once every 256 T-cycles (16384 Hz).
* **Writing DIV resets the WHOLE 16-bit counter** (low bits included).
  Games use this to phase-align timers.

Because TIMA's clock is tapped out of this same counter, a DIV write can
disturb TIMA on hardware: resetting the counter yanks every tapped bit to
zero at once. Our model observes the reset through the per-block sampler,
so such an interaction appears as a sampled 1->0 fall exactly like a
natural one.

## TAC ($FF07): gate + tap selector

| bits | meaning |
|------|---------|
| 2    | timer enable (the gate) |
| 1-0  | which counter bit feeds the TIMA edge detector |

Exact bit-select table — memorize it:

| TAC 1-0 | tapped bit | tick period | rate |
|---------|-----------|-------------|------|
| 00      | 9         | 1024 T-cycles | 4096 Hz |
| 01      | 3         |   16 T-cycles | 262144 Hz |
| 10      | 5         |   64 T-cycles | 65536 Hz |
| 11      | 7         |  256 T-cycles | 16384 Hz |

Bit n toggles every $2^n$ internal counts = $2^{n+1}$ T-cycles per full
period, so it FALLS once per period. TIMA increments on each falling edge
of the tapped bit while the gate is open.

### Model contract (deterministic)

* The tapped bit is SAMPLED once per 4-T-cycle block, gated or not.
  Sampling never stops; it is a pure observer.
* While gated, a sampled 1->0 transition increments TIMA.
* **Enabling mid-tick:** if the tapped bit already reads 1 when you set
  bit 2, its next natural fall still counts — sampling never stopped.
* **The disable edge:** clearing bit 2 while the tapped bit reads 0 AFTER
  the write produces exactly ONE increment. On silicon the enable AND-gate
  output falls at the write itself; our model reproduces that with one
  synchronous increment in `write_tac`.
* **Select-change glitches:** changing bits 1-0 while running can make the
  sample stream appear to fall (old tapped bit 1 -> new tapped bit 0);
  that counts as an edge too, matching hardware's mux behavior.

Real hardware generates these edges from analog mux transitions with
sub-cycle timing; emulators must pick a rule and document it. This is
ours, and every test and golden in the chapter enforces it.

## Overflow: reload, raise, and the missing four cycles

When TIMA wraps $FF->$00:

* **TMA ($FF06)** is copied into TIMA. Writing TMA affects only the NEXT
  reload.
* **IF bit 2** is raised (see below).

On real DMG hardware both take effect **4 T-cycles after the wrap**: for
one instruction window TIMA reads $00 before snapping to TMA, and the IF
raise lands inside that window. We implement the **immediate reload** —
pulse set on wrap, reload + raise applied in the same boundary by
`settle_overflow`. It is a documented deterministic simplification: cycle
counts of everything else are unaffected, only that read-back window
differs. Mooneye-class acceptance tests can observe the real window; that
is what the optional `requires_rom` case is for.

## IF / IE and dispatch priority

* **IF ($FF0F)**: pending lines. Upper 3 bits read 1.
* **IE ($FFFF)**: enabled lines.

| bit | line | vector | typical source |
|-----|------|--------|----------------|
| 0 | VBlank | $40 | PPU start of VBlank (ch14) |
| 1 | STAT | $48 | LCD status modes/LYC (ch14/15) |
| 2 | Timer | $50 | TIMA overflow — this chapter |
| 3 | Serial | $58 | transfer complete (ch18+) |
| 4 | Joypad | $60 | button press (later) |

Dispatch priority is bit order: bit 0 wins over bit 2 if both pend. One
dispatch = clear that IF bit, push PC, PC=vector, IME=0, 20 cycles
(5 M-cycles). The IME/EI-delay/RETI semantics are exactly chapter 11's;
`01_divider/int_ctl.hpp` carries them unchanged.

HALT wakes when IE & IF != 0. With IME=0 the CPU resumes WITHOUT
servicing: execution continues at the next instruction and the IF bit
stays set — the classic "wake, check, re-halt" polling pattern.

## Machine ordering

The driver executes one instruction boundary at a time:
execute -> tick the divider through the instruction's cycles -> service
interrupts -> tick the divider through the 20 entry cycles. Event logs
(91_challenge) print at the boundary's end cycle count. Everything is a
multiple of 4 T-cycles, so goldens are byte-stable.
