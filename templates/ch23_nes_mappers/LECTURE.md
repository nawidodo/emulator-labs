# Lecture — NES Mappers

Your CPU can only see 32 KiB of PRG at `$8000-$FFFF` and the PPU only 8 KiB
of CHR — yet games ship hundreds of kilobytes. A **mapper** is the cartridge
chip that answers every CPU/PPU read with a *window* into a much larger
image, re-aiming those windows on command. Bank switching is not a CPU
feature; it lives on the board.

## The iNES container

Before any banking you must know what is on the board. The iNES header
(16 bytes) says how many 16 KiB PRG banks (byte 4), 8 KiB CHR units
(byte 5), whether a 512-byte trainer precedes PRG (flags6 bit 2) and the
nametable arrangement (flags6 bit 0; bit 3 = four-screen overrides it).
The mapper number merges TWO nibbles: high nibble of flags6 | flags7's
high nibble: `mapper = (flags6 >> 4) | (flags7 & 0xF0)`. Get this wrong
and you emulate the right board badly or the wrong board entirely.

## UxROM (mapper 2)

One latch, written by ANY CPU write into `$8000-$FFFF`:

```text
$8000-$BFFF : PRG bank selected by the latch (16 KiB window)
$C000-$FFFF : LAST bank, hard-wired   <- reset code always lives here
CHR         : flat writable 8 KiB RAM on the board
```

The fixed last bank is the whole design: code resets into it, then swaps
data windows below freely. Mask the written value to installed banks —
games routinely write bank numbers past the top of small images.

## CNROM (mapper 3)

PRG never switches. The single register selects an 8 KiB CHR ROM unit:

```text
any CPU write $8000-$FFFF : chr_bank = value & (chr_units - 1)
PPU $0000-$1FFF           : chr[chr_bank * 0x2000 + addr]
```

Cheap board, but note what it teaches: mapper registers are *decoded
writes*, not memory cells. Nothing is stored at `$8000`; the write pulse
is the interface.

## MMC1 (mapper 1): serial loading

MMC1 has no direct registers either — it has a 5-bit shift register.
Every CPU write to `$8000-$FFFF` feeds data bit 0; on the fifth write the
accumulated bits are dispatched to an internal register chosen by WHICH
ADDRESS WINDOW received the fifth write:

| Window      | Register                                        |
|-------------|-------------------------------------------------|
| `$8000-$9FFF` | Control: CHR mode (bit4), PRG mode (bits3-2), mirroring (bits1-0) |
| `$A000-$BFFF` | CHR bank 0                                    |
| `$C000-$DFFF` | CHR bank 1                                    |
| `$E000-$FFFF` | PRG bank                                      |

A write with bit 7 set flushes the shift register — and hardware also ORs
`0x0C` into Control, snapping PRG mode back to "fix last". Games flush
before loading; your tests should too.

PRG modes: `0/1` map a 32 KiB window at `$8000` (bank bit 0 ignored);
`2` fixes bank 0 low and switches `$C000`; `3` switches `$8000` and pins
the last bank high. Mirroring encoding differs from iNES here:
`0`=one-screen lower, `1`=one-screen upper, `2`=vertical, `3`=horizontal.

## MMC3 (mapper 4): banking + the scanline IRQ

Register decode uses address parity inside four ranges:

```text
$8000-$9FFF even : command latch (bits 2-0) + invert flags (bit6 CHR,
                   bit5 PRG)
$8000-$9FFF odd  : bank data R0-R7
$A000-$BFFF even : mirroring, bit0: 0=vertical, 1=horizontal (INVERTED
                   vs iNES!)
$C000-$DFFF even : IRQ latch        odd : IRQ reload request
$E000-$FFFF even : IRQ disable+ack  odd : IRQ enable
```

R0/R1 are 2 KiB CHR banks, R2-R5 are 1 KiB, R6/R7 are 8 KiB PRG. One PRG
slot is always fixed second-to-last, one fixed last; the inversion flags
shuffle which slot R6 occupies and where the 2 KiB CHR pairs sit.

### IRQ model (course-simplified, deterministic)

Real MMC3 clocks its counter on PPU A12 rising edges through an analog
filter (~12 dots of hysteresis). Our model keeps the digital skeleton:
the harness notifies the mapper of every qualifying rising edge
(`a12_edge()`), and the counter obeys exactly three rules:

1. reload requested? -> `counter = latch`, clear request;
2. else if `counter == 0` -> `counter = latch`, assert IRQ if enabled;
3. else decrement.

Consequences worth internalizing:

- period = latch + 1 edges (latch stores "period minus one");
- the assertion is LEVEL-held until an `$E000` write acknowledges;
- rewriting the latch mid-countdown does NOT touch the running counter —
  the new period lands at the next reload. This is the seeded bug of
  `90_debug` and the heart of `91_challenge`.

## Mapper IRQ -> CPU IRQ

A mapper IRQ is simply another open-collector line into the CPU's IRQ
input alongside APU frame/DMC interrupts. Level-held semantics mean the
CPU sees the line until something acknowledges it; games poll-and-clear
or take the vector directly. In this chapter the harness reads
`irq_line()`; in a full emulator it would gate interrupt polling after
each instruction.

## References

NESdev wiki: *iNES*, *UxROM*, *CNROM*, *MMC1*, *MMC3* pages. blargg's
`mmc3_test` ROMs document the real-A12 behaviors our simplified model
deliberately abstracts; the model is still exact enough to reproduce
period arithmetic and reload timing bug-for-bug.
