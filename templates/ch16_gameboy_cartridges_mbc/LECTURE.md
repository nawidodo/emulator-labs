# ch16 — Cartridges and Memory Bank Controllers (MBC)

The Game Boy's CPU can address only 64 KiB, yet carts shipped up to
8 MiB of ROM and 128 KiB of SRAM. The trick is **bank switching**: a
chip on the cartridge — the *Memory Bank Controller* — watches writes
the CPU makes to ROM addresses ($0000-$7FFF, which normally cannot
change ROM contents!) and latches them into bank registers. Those
registers choose which 16 KiB *banks* of the mask ROM are visible in
the CPU's address window at any instant.

## The map as seen by the CPU

```text
$0000-$3FFF   ROM bank 0 area      MBC1 mode 1 can re-map this half
$4000-$7FFF   ROM bank N area      switched: MBC1 5 bits, MBC3 7 bits,
                                   MBC5 full 9 bits
$A000-$BFFF   cart SRAM / RTC      gated by a RAM-enable register
$0000-$7FFF   (writes)             MBC register windows — never reach ROM
```

## The register windows

Every write to $0000-$7FFF lands in the cartridge, not the ROM:

| Window       | MBC1                     | MBC3                       | MBC5                          |
|--------------|--------------------------|----------------------------|-------------------------------|
| $0000-$1FFF  | RAM enable (`val&F==$A`) | RAM enable (`val&F==$A`)   | RAM enable (`val&F==$A`)      |
| $2000-$3FFF  | bank1 = low 5 bits, 0->1 | bank = low 7 bits, 0->1    | ROM bank low 8 bits           |
| $4000-$5FFF  | bank2 = low 2 bits       | RAM bank 0-3 / RTC $08-$0C | RAM bank 0-15                 |
| $6000-$7FFF  | banking MODE latch       | RTC latch handshake        | (unused)                      |
| $3000-$3FFF  | —                        | —                          | sets ROM bank bit 9           |

Two quirks worth internalizing:

* **Bank 0 wraps to 1** on MBC1 ($2000-$3FFF write of 0 selects bank 1)
  and MBC3; on MBC5 bank 0 is genuinely selectable.
* **Out-of-range selects never open-bus.** The hardware masks the
  selected bank modulo the real ROM size, so a game that writes $FF on a
  32-bank cart silently gets bank 31.

## MBC1 banking MODE

MBC1 has two extra ROM address lines fed by `bank2`. In **mode 0**
(the power-up default) they extend the *high* half: physical bank =
`(bank2<<5) | bank1`. In **mode 1** they also re-map the *low* half to
physical bank `bank2<<5` and select one of four SRAM banks. Games with
exactly 512 KiB / 1 MiB and 32 KiB SRAM use mode 1 to reach "impossible"
combinations.

## MBC3 and the real-time clock

MBC3 carts can carry an RTC: five registers (seconds, minutes, hours,
day low byte, day high byte). Writes $08-$0C to $4000-$5FFF redirect
$A000-$BFFF from SRAM to those registers. Day-high bit 6 halts the
clock; bit 0 is day-count bit 8.

Real hardware ticks the clock continuously even when the console is off
(that is what the battery is for). Our lab model is deliberately
different and deterministic: **time advances only when tick(cycles) is
injected**, with 4194304 T-cycles = 1 second and a carry chain
secs->mins->hours->days->day-bit-8. Sub-second remainders drop per call.

The **latch procedure** freezes the clock for consistent reads: write
$00 then $01 to $6000-$7FFF and the live registers are copied into
shadow registers; reads come from the shadows until the next latch.
Repeated $00 writes keep the machine armed; any other order does not
latch.

## MBC5

Dropping the mode register entirely, MBC5 gives games a clean 9-bit ROM
bank number split across two windows: $2000-$2FFF holds the low 8 bits,
$3000-$3FFF holds bit 9. Selecting bank 0 is legal. RAM banks run 0-15
with no mode quirk.

## Battery SRAM

Battery-backed SRAM survives power loss. An emulator persists it by
writing the exact RAM array to disk (`.sav` file, no header), sized by
header code $0149. On load, refuse truncated files rather than
corrupting live state.

## Header essentials (exercise 01)

| Offset       | Meaning                                            |
|--------------|----------------------------------------------------|
| $0134-$0143  | title, 16 raw bytes                                 |
| $0147        | cartridge type -> mapper + peripherals              |
| $0148        | ROM size code: bytes = 32 KiB << code ($52-$54 oddballs) |
| $0149        | RAM size code                                       |
| $014D        | header checksum: sum($134..$14C) + $014D + 25 == 0 mod 256 |

The emulator reads exactly these bytes to pick a mapper strategy and
size its SRAM before a single opcode runs.

## Design lens: mappers as strategies

All MBCs answer the same four questions — read ROM, read RAM, decode a
register write, write RAM — so chapter 16 models them behind one small
interface (`Mapper`) with concrete strategies built by a factory
(`CartridgeController::makeMapper`) that dispatches on header type $147.
That is the same polymorphism your full emulator will use when the CPU
asks its memory bus for a read.
