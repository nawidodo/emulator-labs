# Chapter 9 Coding Test — Board Bring-Up for the "Arcade-8080-B"

You have never seen this machine. It runs the same 8080 CPU from chapters
7–9, but everything around it is new. Implement it **from the board
document below** — the hidden grader boots unseen programs against your
construction and checks final RAM state and frame hashes.

## Board document: Arcade-8080-B

### Memory map

```text
0000-3FFF   ROM   16 KiB = eight independent 2 KiB banks, linear,
                  writes ignored, unprogrammed tail reads 0xFF
4000-43FF   RAM    1 KiB work RAM
4400-5FFF   VRAM   7 KiB 1bpp framebuffer (same column-major layout as
                  the Space Invaders board; same upright renderer)
```

Unmapped reads float low (0x00); unmapped writes drop.

### Timing and interrupts

```text
clock       2160 kHz -> exactly 36000 T-states per frame at 60 Hz
interrupts  dual one-shot vblank timers alternating per frame:
            even frame jams RST vec 0x18 (opcode DF)
            odd  frame jams RST vec 0x30 (opcode F7)
```

### I/O ports

| Port | Dir | Function |
|---|---|---|
| IN 0/1/2 | R | input latches 0/1/2 (same bit meanings as chapter board) |
| OUT 2 | W | shift amounts: bits 0-2 -> shifter #1, bits 3-5 -> shifter #2 |
| OUT 4 | W | sound event recorder A |
| OUT 5 | W | sound event recorder B |
| OUT 6 | W | shifter #1 data write (same byte-shift semantics as ch09) |
| IN 6 | R | shifter #1 result |
| OUT 7 | W | shifter #2 data write (a second, identical TTL part) |
| IN 7 | R | shifter #2 result |
| OUT 0 | W | watchdog kick |

Both shift registers are the exact 8-bit hardware part from exercise 2:
16-bit register, write shifts old high half to low half and inserts the
new byte high; read returns bits `[amount .. amount+7]`.

## Your task

Complete the four `TODO(n)` blocks in `machine_b.hpp`: memory windows,
timer configuration, IN decode and OUT decode — all derived from the
`MachineSpec`, never hard-wired. The runner (`ch09_99_si_b_runner`) then
boots B-board programs:

```bash
./build/skels/ch09_space_invaders_machine/99_coding_test/ch09_99_si_b_runner \
    --rom prog.bin --frames N [--input-file FILE] [--trace FILE] \
    [--hash-frame FILE] [--dump-state FILE]
```

## Traps

- The ROM is twice as large as the chapter board: bank splitting must use
  the spec, not a constant.
- Two shift registers share ONE amount latch port. Mask carefully.
- Port numbers overlap between directions: IN 6 vs OUT 4 are unrelated
  decoders.
- The vectors live at 0x18 / 0x30, not 0x08 / 0x10 — programs jump
  through them.
