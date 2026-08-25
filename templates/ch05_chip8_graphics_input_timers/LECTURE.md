# Chapter 5 — CHIP-8 Graphics, Input and Timers

Learn: framebuffers, XOR sprites, collision flag, wrapping/clipping, key
state, 60 Hz timers, host timing, and — the thread that ties this chapter to
every later one — **testing an emulator without opening a window**.

## Framebuffers

A CHIP-8 display is a monochrome 64x32 grid. The honest data structure is a
flat array, not something nested:

```cpp
bool pixels[64 * 32];   // pixels[y * 64 + x]
```

Flat layout matches how display hardware scans out (row-major) and how
emulator backends blit, so exercise 01 builds exactly that with a bounds API:
out-of-bounds reads are `false`, out-of-bounds writes are dropped. Every other
component can then be written without carrying bounds checks around.

## XOR sprites and the collision flag

`DXYN` draws an 8-pixel-wide sprite of `n` rows at `(Vx, Vy)`:

```text
pixel = screen_pixel XOR sprite_bit
VF    = 1 if any LIT pixel was turned OFF by the XOR, else 0
```

Two asymmetries matter and both are load-bearing for real games:

1. **Collision means erasure.** Lighting empty pixels never sets VF; only
   overwriting lit pixels does. Breakout-style games detect "ball hit brick"
   purely from this flag.
2. **Redraw = toggle.** Drawing the same sprite twice erases it — the cheap
   sprite-erasure trick every CHIP-8 animation loop uses.

## Wrapping vs clipping

When a sprite crosses the screen edge, documented hardware did either of:

- **clip**: off-screen pixels are dropped; visible parts still draw;
- **wrap**: coordinates are taken modulo the screen size; sprites re-enter
  the opposite edge.

The course default is clip; the alternative lives behind
`Chip8Quirks::wrapping` so chapter 6 can flip behaviours per game. Keep the
policy decision in ONE function (`locate_pixel`) instead of sprinkling
conditionals through the draw loop.

## Key state

The keypad is 16 bits of live state: `down[16]`. Two instructions read it:
`EX9E`/`EXA1` skip based on key state, and `FX0A` blocks until A key is held.
Headless testing replaces the human with a **scripted input feed**: one line
per frame listing held hex digits (`.` = none). Applying a feed frame REPLACES
keypad state, which makes replays bit-exact no matter how long earlier frames
took. Where real hardware was ambiguous (several keys held during FX0A), we
pick the lowest-numbered key and document it — ambiguity is the enemy of
golden hashes.

## 60 Hz timers and host timing

Delay and sound timers count down at 60 Hz independently of instruction
execution. The classic implementation mistake is tying decrements directly to
instruction steps; the correct shape is a fractional accumulator:

```text
accumulator += cycles * 60
while accumulator >= cycles_per_second:   # whole ticks due
    decrement delay/sound; accumulator -= cycles_per_second
```

This chapter fixes the teaching rates at **600 CPU cycles/second**, 60 Hz
timers, 60 fps — so 10 instructions per timer tick and per frame. Real hosts
ran ~500-1000 ips; picking ONE value is what makes headless replays
deterministic. The beep is never audible: `Timers::on_beep` fires on sound
transitions and observers record them.

## Separating CPU rate from timer rate

`run(n_cycles)` couples them (ticks fall out every 10 cycles). The coding
contract `run_for(cycles, timer_ticks)` decouples them completely: exactly N
instructions AND exactly M decrements, independent of each other. This is the
pattern every later machine uses when its timers drift from its CPU clock.

## Testing without a window (§52-53)

From this chapter on, every emulator is testable headlessly. The runner CLI
is fixed across all systems:

```bash
emu --rom FILE [--headless] [--cycles N | --frames N] [--trace FILE]
         [--hash-frame FILE] [--input-file FILE]
```

Frames become **RGBA8888 dumps** (ON -> white, OFF -> black) digested with
FNV-1a 64 — identical algorithm in C++ and in `tools/labs/hash_frame.py`, so
manifest hashes always agree. One static frame hash catches drawing bugs; a
**frame-sequence hash** (one digest per executed frame, `--frame-hashes`)
catches timing bugs that only show up as motion drift — which is why the
animated fixtures matter even though their final frame looks innocent.
