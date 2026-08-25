# Provenance — ch20 public ROMs

All images are synthetic, course-original bytes produced by the committed
generator `templates/ch20_nes_bus_cartridges/02_ines_nrom/tools/gen_fixtures.py`
(pure python, no external data; nothing extracted from a commercial ROM).

## roms/boot_success.nes

Mapper-0 NROM: 16KB PRG / 8KB CHR, flag6 = $00 (horizontal arrangement).
Reset code performs the standard init dance (SEI CLD LDX #$FF TXS), writes
the success code `$77` to RAM `$02` and witness `$55` to `$03`, then loops
on `INC $04`. Vectors NMI/RESET/IRQ all point at the init block.

Reference run (twice, byte-identical RAM dumps):

```
ch20_91_boot_runner --rom boot_success.nes --headless --cycles 500 \
    --dump-ram ram.bin --expect-ram 02=77 --expect-ram 03=55
```
FNV-1a 64 of the 2KB dump after 500 cycles: 5AD15AD0E7E8576D.

## roms/vertical.nes

Identical image with flag6 = $01 (vertical arrangement) — used to exercise
the mirroring decode path end-to-end.

## roms/chrram.nes

CHR bank count 0: mapper-0 board with 8KB CHR-RAM instead of CHR ROM
(the coding-test variant). Same PRG as boot_success.nes.
