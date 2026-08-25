# 02 — Mini devices under the scheduler

Stand-in SoC: a tiny synthetic RISC core plus GPU / DMA / CD / SPU /
INTC models, every one of them advanced ONLY by scheduler events.

## Master clock and ratios

| Clock        | Value        | Relationship to CPU (33.8688 MHz)      |
|--------------|--------------|----------------------------------------|
| GPU dot clock| 53.2224 MHz  | exactly 11/7 x CPU -> 7/11 cycle/px    |
| SPU sample   | 44100 Hz     | exactly 768 CPU cycles per sample      |
| Video frame  | 60 Hz (NTSC) | 564480 CPU cycles (runner --frames)    |
| CD sector    | synthetic    | 19968 cycles = exactly 26 samples      |

Fractional ratios use Bresenham integer accumulators — no floating point
anywhere in the timing path.

## Devices

- **GPU** — `GP0` writes queue command words; a drain event executes them
  at the pixel ratio; STAT bit 28 = busy; optional IRQ on idle (line 1).
- **DMA** — drains N words at 6 cycles/word; while draining the next CPU
  event is scheduled at the drain deadline: the stall, structurally.
  Completion raises INTC line 3.
- **CD** — one outstanding read; completes after 19968 cycles, raises
  line 2. The latency is an exact multiple of the SPU sample period so
  CD/SPU deadlines can be aligned to exercise tie-breaks.
- **SPU** — sample-period chain anchored at deadline+768 forever; while
  enabled each boundary latches line 9.
- **INTC** — status/mask latch; write to I_STAT acks. Latch ORDER across
  simultaneous events is guaranteed by the scheduler's FIFO tie-break.

## Mini core

LUI/ORI/ADDIU/ANDI/SW/LW/BNEZ/J/HALT, fixed 4-cycle cost, word-indexed
pc, little-endian `.bin` images loaded at word 0. Encoding table lives in
`core.hpp`; committed fixtures carry a matching `.asm.txt`.
