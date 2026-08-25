# Chapter 33 — SNES DMA, HDMA and the Audio Subsystem

You have built single-clock machines so far. The SNES is a *system*: a
65816 CPU, a PPU, an SPC700+DSP audio pair, all running from one crystal at
different divisors, shovelling data into each other's registers behind the
scene. This chapter models the three mechanisms that make it tick.

Sources used throughout: Anomie's *SNES hardware register list*
(<https://github.com/gilligan/snesdev/blob/master/docs/snes_registers.txt>)
and the SNESdev Wiki pages on DMA, HDMA, and the APU
(<https://snes.nesdev.org/wiki/DMA>).

## 1. DMA — eight channels, eight patterns

DMA is a brute-force register pump: the CPU programs a channel, sets one
"start" bit, and hardware moves bytes one by one through the B-bus
($2100-$21FF, the PPU/APU register page) without CPU involvement.

Channel `x` (0-7) owns ten registers `$43x0`-`$43xA`:

| Register | Name | Meaning |
|---|---|---|
| `$43x0` | DMAPx | control byte (below) |
| `$43x1` | BBADx | B-bus register low byte (`$2100 | BBADx`) |
| `$43x2-3` | A1TxL/H | A address |
| `$43x4` | A1Bx | A bank |
| `$43x5-6` | DASxL/H | unit count / indirect count |

Control byte layout (Anomie):

```text
bit:    7        6        5   4-3          2-0
        dir      hdma     -   a-step       mode
        0=A->B   ind.         00 inc       pattern 0-7
        1=B->A                x1 fixed
                              1y dec/fixed
```

The **transfer-mode table** — this exact table is normative for exercise
01 (SNESdev Wiki "DMA", transfer pattern field; identical numbers in
Anomie):

| Mode | Units per transfer | B-bus offsets | Typical use |
|---:|---:|---|---|
| 0 | 1 | +0 | WRAM fills, Mode-7 data |
| 1 | 2 | +0,+1 | VRAM via `$2118/$2119` |
| 2 | 2 | +0,+0 | CGRAM/OAM word writes (`$2122,$2122`) |
| 3 | 4 | +0,+0,+1,+1 | BG scroll pairs |
| 4 | 4 | +0,+1,+2,+3 | window registers |
| 5 | 4 | +0,+1,+0,+1 | undocumented |
| 6 | 2 | +0,+0 | like mode 2, but FORCES A decrement |
| 7 | 4 | +0,+0,+1,+1 | like mode 3, but FORCES A decrement |

Two things students routinely conflate:

1. **The pattern selects only B-register addresses.** Whether the A
   pointer moves is bits 4-3, independently.
2. **Modes 6/7 hijack the A step.** They exist to give games a
   decrementing transfer without spending control bits; bits 4-3 are
   ignored when mode is 6 or 7.

## 2. HDMA — DMA that fires every scanline

HDMA reuses the DMA channel registers but runs once per scanline,
automatically, reading a *table* from RAM:

```text
header byte:  bit7 = repeat flag, bits 0-6 = line count
  $00            -> channel terminates for the frame
  count N, b7=0  -> fresh table data fetched EVERY line for N lines
  count N, b7=1  -> data fetched once; N lines reuse it ("repeat")
```

Direct tables carry their data inline. Indirect tables (DMAPx bit 6)
carry a little-endian pointer instead, and the data lives elsewhere in
RAM (banked in hardware via `$43x7`).

**The timing rule this chapter is built around:** HDMA writes land at the
*start* of a scanline, so line N renders with line N's data. Games build
per-line gradients by giving every line its own one-byte entry. Get this
wrong by one line and gradients visibly shift down — which is precisely
the seeded bug of exercise 90_debug.

Simplifications we adopt (documented, deliberate): one flat RAM image for
tables and indirect data instead of banked addressing, and each channel
writes one base register plus up to three consecutive registers rather
than full DMA-mode patterns.

## 3. The audio subsystem and ports $2140-$2147

The SPC700 + DSP pair is a separate computer with its own 64 KB APRAM.
It has no direct access to game code; everything crosses four **dual
8-bit communication ports** `$2140-$2147`. Each port is two registers:
one seen by the CPU, one seen by the SPC700. Uploads are handshake-driven:
the CPU writes a magic value to `$2140`, then streams bytes while the
SPC700 polls.

Real uploads run a state machine inside the SPC700 program (Nintendo's
$2140 protocol uses values $A0-$BF for direction and counter). Our stub
APU keeps the shape and removes the timing trap: every CPU write is queued
in a FIFO, and `consume()` drains it deterministically:

```text
CPU writes $2140 = $A0 | slot      (handshake; slot 0..15)
CPU writes $2141 = length          (bytes to follow, 0..255)
CPU writes `length` bytes rotating across $2141/$2142/$2143
consume() commits the block to APRAM at slot * 4096
```

The DSP is simplified to what tests can pin exactly: per-voice 8-bit
volume applied as `(sample * volume) >> 8`, a 7-bit master volume applied
as `(mix * master) >> 7` with int16 clamping between stages, and echo
**disabled** — no echo buffer exists at baseline. Documented deviations,
not accidents.

## 4. Three domains, one timebase

The master clock is 21477272 Hz. Real dividers: CPU = master/6, PPU dot =
master/4, SPC700 ≈ master/21.47. We model:

```text
cpu_ticks = master / 6      (exact)
ppu_ticks = master / 4      (exact)
apu_ticks = master / 32     (SIMPLIFICATION: real ~master/21.47;
                             power-of-two chosen so integer accumulators
                             stay drift-free and reproducible)
```

Every domain gets an integer accumulator: add the master delta, emit
`floor(acc/divisor)` ticks, keep the remainder. Remainders must survive
across calls or domains drift apart — test `ClockDomains.RemaindersCarry`
in exercise 03 fails the moment you lose one.

Per-frame ordering contract (used by the challenge runner):

```text
frame start -> hdma_init()          (rewind every channel table)
for each visible line n:
    hdma line effects for line n    (writes visible DURING line n)
    ppu draws line n
```

HDMA before draw, every line, no exceptions.

## Exercises

| Exercise | You implement |
|---|---|
| `01_dma` | the canonical 8-mode table, A-step decode incl. modes 6/7 override, full bus-transfer sequences |
| `02_hdma` | header parsing, direct/indirect fetch, per-line engine with at-line-start semantics |
| `03_apu_sync` | comm-port upload FSM, DSP scaling math, integer domain dividers |
| `90_debug` | find and fix a one-scanline-late HDMA effect; write `bug-report.md` |
| `91_challenge` | wire bundle config into the HDMA core, produce the golden effect buffer |
| `99_coding_test` | implement the unseen "transfer mode X" from SPEC.md cold |
