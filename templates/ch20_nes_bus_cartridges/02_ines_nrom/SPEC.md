# 02 — iNES Cartridges and NROM

Above $4017 the bus is a cable to the game pak. This exercise defines the
connector (the `Mapper` interface), reads the iNES header that describes
what's plugged in, and implements mapper 0 — NROM — the simplest cartridge
ever shipped.

## Tasks

1. **`Header::parse`** — magic, PRG/CHR bank counts, mirroring bit (flag6
   bit0: 0 = horizontal, 1 = vertical; bit3 = four-screen), battery and
   trainer flags, mapper number from flag6>>4 | flag7 high nibble.
2. **NROM PRG mapping** — one 16KB bank appears at BOTH $8000-$BFFF and
   its mirror $C000-$FFFF (the reset vector at $FFFC must work on a 16KB
   cart!); two banks map linearly. PRG writes are dropped.
3. **PPU side + mirroring** — CHR below $2000; nametable traffic lands in
   onboard CIRAM through `mirror_translate()` per the header's hard-wired
   layout.

## Fixture generator

`tools/gen_fixtures.py` builds the committed synthetic ROMs under
`tests/public/ch20_nes_bus_cartridges/roms/` (boot program, vertical-
mirroring variant, CHR-RAM variant). No commercial images.

## Acceptance

- All `ines.*`, `mirror.*`, `nrom.*` tests pass.
