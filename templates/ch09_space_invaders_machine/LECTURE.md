# Chapter 9 — Space Invaders Machine

Chapters 7 and 8 built a CPU. This chapter builds a **machine**: the CPU
drops into a board with its own memory map, its own peripherals, its own
interrupt sources — the classic Space Invaders architecture (course
reimplementation; no commercial ROMs).

## The memory map

```text
0000-1FFF   ROM   8 KiB = four 2 KiB banks, linear, masked
2000-23FF   RAM   1 KiB
2400-3FFF   VRAM  7 KiB framebuffer
```

Three device objects route through one decoder. The board mirrors
NOTHING: each range covers its window exactly once. When a bus read
misses every window the line floats low (0x00); writes vanish. These are
documented stand-ins for floating-bus behavior — the point is that they
are *specified*, not accidental.

## I/O: a second address space

The 8080's IN/OUT instructions put a port number on the address lines and
nothing else. All behavior lives in the machine:

| Port | Dir | Function |
|---|---|---|
| IN 0/1/2 | R | input latches (coin/service, buttons, dips) |
| IN 3 | R | shift-register result |
| OUT 2 | W | shift amount (bits 0-2) |
| OUT 3/5 | W | sound events (documented stubs → event log) |
| OUT 4 | W | shift-register data |
| OUT 6 | W | watchdog kick |

Input is scripted: a text file with three hex latch bytes per frame feeds
the latches during headless runs (`--input-file`), which is what makes
input-driven goldens reproducible.

## The shift register — why it exists

Blitting an 8-pixel-wide sprite from a byte was expensive on 1978
hardware, so the board has a dedicated TTL part: a 16-bit register you
fill two bytes wide (OUT 4 shifts everything right by a byte and inserts
the new byte high — write LOW first, HIGH second) and read back as an
8-bit window slid along by a 3-bit amount counter (OUT 2 bits 0-2, wraps
mod 8). One write + one amount change = sprite positioned to the pixel.
Every behavior is exhaustively testable without a CPU in sight.

## Video RAM

7 KiB of 1bpp memory, COLUMN-major: byte `(col*32 + y/8)` holds the eight
pixels of column `col` between rows `y&~7` and `y|7`, bit `y%8` selects
within the byte. The cabinet monitor is mounted rotated 90°; rendering
"upright" 224×256 means x = column, y = row — the rotation lives entirely
in this decode. Get the byte index factors wrong and you get the most
famous emulator bug of all: a perfectly transposed picture.

Frames render to RGBA8888 (white on black) and hash with FNV-1a 64 —
golden frames travel as hashes, not images.

## Interrupt generation

Two one-shot timers alternate per frame period: even frames jam **RST 08**
onto the bus, odd frames jam **RST 10**. The CPU samples between
instructions; acceptance pushes PC and clears IFF exactly like chapter 8.
A masked CPU simply loses the pulse — these are edge pulses, not latches.

The documented timing model: fixed 1920 kHz clock → exactly 32000
T-states per frame at 60 Hz. Frame boundaries land on clean cycle counts,
so cadence bugs show up as off-by-one counters instead of drifting chaos.

## Machine-specific hardware

Sound ports are stubs that append `(cycle, port, value)` events to a log —
the log IS the observable contract, and it is fully assertable in tests.
The watchdog records kicks; expiry is queryable but never fires mid-test.

## Testing philosophy

Curriculum §52–59 in miniature:

- every device constructs alone (§57),
- every peripheral contract gets exhaustive specification tests (§58),
- integration = run a real program headless, compare traces/hashes (§52),
- debugging = seeded defects found by FIRST divergence (§54).

## References

- Space Invaders architecture notes (ports, shifter, rotated VRAM)
- Intel 8080 Assembly Language Programming Manual — IN/OUT timing
