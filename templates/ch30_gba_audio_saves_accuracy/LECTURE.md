# Chapter 30 — GBA Audio, Saves and Accuracy

Primary reference: [GBATEK — Sound Controller](https://problemkaputt.de/gbatek.htm#soundcontroller),
[Cartridge Savegame Memories](https://problemkaputt.de/gbatek.htm#cartridgesaves),
[RTC](https://problemkaputt.de/gbatek.htm#rtcregisters).

The final GBA chapter ties three accuracy-critical areas together and ends
with the milestone requirement: a fully headless deterministic test runner.

## Legacy PSG channels

Four Game Boy–style channels plus two Direct Sound channels:

```text
CH1  square + frequency sweep     CH2  square
CH3  4-bit wave playback          CH4  LFSR noise
```

Each square channel: duty cycle (12.5/25/50/75%), length counter (64-step,
256 Hz frames), volume envelope (per-tick add/sub toward 0 or 15, 64 Hz
frames), frequency with 4/8-bit sweep for CH1. Noise uses a 15-bit LFSR;
when the "power-down" divisor register selects the short mode the effective
register width collapses to 7 bits, producing metallic bursts.

Routing: NR50 holds left/right master volumes (3 bits each), NR51 is the
per-channel L/R routing nibbles. Mixing is additive into a small integer
range; everything downstream adds bias (below).

## Direct Sound channels A/B

Each channel owns a 32-byte FIFO. Sound DMAs (DMA1/DMA2 special trigger)
refill four words (16 bytes) whenever occupancy drops to half. A selected
timer overflow pops ONE byte per channel as the current sample:

```text
sample = s8(FIFO byte) >> (shift)      shift from SOUNDCNT_H bits 0-1/8-9:
                                         0 -> 25%, 1 -> 50%, 2 -> 100%
output = clamp(psg_left/right + dsoundA + dsoundB + bias)
```

Writing bit 7/15 of SOUNDCNT_H resets a FIFO (clears it so the next DMA
starts clean). SOUNDBIAS (0x04000088) adds a 10-bit DC offset and its
resolution field quantizes the output grid — audible as dithering noise if
emulated wrong. Our model quantizes deterministically.

## Cartridge saves

```text
SRAM    32 KiB byte-array, direct reads/writes at 0x0E000000
Flash   512 B/page command-driven chip: 64 KiB (single bank) or
        128 KiB (two banks switched by command B0)
EEPROM  512 B or 8 KB, bit-banged serial protocol driven by DMA3
RTC     over GPIO lines at cartridge addresses C4/C6/C8 (documented)
```

### Flash command state machine (state-exact)

```text
AA 55 90       enter chip-ID mode        reads: 00->mfg, 01->device
AA 55 F0       leave ID mode
AA 55 80       arm erase
AA 55 10       chip erase (all FF)
AA 55 30 @sa   sector erase (4 KiB page containing sa)
AA 55 A0 d@a   program one byte — AND semantics: new = old & d
AA 55 B0 ba    select bank 0/1 (128 KiB devices only)
```

Two classic emulator bugs this chapter hunts: accepting ID-mode entry from a
wrong prefix sequence, and programming bytes with assignment instead of the
AND mask (flash can only flip 1-bits to 0 until an erase restores them).

## EEPROM protocol

Driven by DMA3 halfword streams where bit 15 of each halfword carries one
bit. Command frame: start bit `1`, operation `10`=read / `00`=write, then
6 address bits (512 B) or 14 bits (8 KB). Writes continue with 64 data bits
and a trailing 0; reads answer with 64 bits after a short ready delay.
Size autodetection uses the transfer length heuristic games rely on.

## RTC over GPIO

The real-time clock hangs off three GPIO registers in the cartridge map:
data (C4), direction (C6), control (C8). Games bit-bang a serial protocol
to read/write clock registers; emulators model the shifter and a virtual
clock seeded from host time — frozen deterministically here (headless
requirement). This chapter documents the flow; implementing it end-to-end
is left as extended study.

## Headless deterministic runner (milestone)

Curriculum §52: every system must run without a window. The chapter's
`suite_runner` executes named subsystem suites (audio / saves / eeprom)
with fixed inputs and prints pass/fail plus digests; `--suite` filters let
you isolate which subsystem fails without touching anything else — exactly
the workflow the coding test drills.
