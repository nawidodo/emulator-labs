# 04 — MBC5 and battery SRAM

| seq | function        | contract |
|-----|-----------------|----------|
| 1   | `writeReg`      | $0000-$1FFF RAM enable nibble; $2000-$2FFF ROM bank low 8 bits (bit 9 preserved); $3000-$3FFF sets/clears ROM bank bit 9 (only bit 0 meaningful); $4000-$5FFF RAM bank = `val & 0x0F`. No mode register, no wrap-to-1: bank 0 is selectable. |
| 2   | `computedBank`  | physical high-half bank = `romBank % nbanks` (the 9-bit select wrapped by the wired address lines) |
| 3   | `readRam`       | gated by the enable nibble; routed through the selected RAM bank, masked to the image's RAM size; disabled/absent -> $FF |
| 4   | `writeRam`      | mirror of read-side banking |
| 5   | `saveSram`      | writes the exact SRAM array to `path` as a header-less `.sav` file; false on I/O failure |
| 6   | `loadSram`      | reads exactly `ramSize()` bytes into a staging buffer and commits only on success; missing/truncated file -> false with live state untouched |

## Acceptance

`ch16_04_mbc5_battery_tests` passes: direct low-byte selects over a
patterned 64-bank image, full 9-bit arithmetic (`romBank9() == 511`,
`computedBank() == 511 % nbanks`) without committing an 8 MiB fixture,
bit-9 preservation across low-window writes, 16-bank RAM routing with no
mode quirk, and an SRAM round-trip through `ch16_sram_test.sav`
(including truncated-file rejection) that cleans up after itself.

## Why unit-test bit 9 instead of shipping an 8 MiB ROM?

The observable contract of the $3000-$3FFF window is one bit in a bank
number. Exposing it through the `computedBank`/`romBank9()` seams lets a
small committed image prove the same math an 8 MiB cart would exercise.
