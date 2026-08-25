# 01 — Cartridge header parsing

Before a mapper is chosen, the emulator reads the fixed header inside
bank 0. Every field this exercise touches is real boot-rom-visible data:

| seq | function              | contract |
|-----|-----------------------|----------|
| 1   | `title`               | 16 raw bytes at $0134, no trimming/case folding |
| 2   | `hasBattery`          | true for types $03,$06,$09,$0D,$0F,$10,$13,$1B,$1E |
| 3   | `controllerName`      | "ROM_ONLY", "MBC1", "MBC1+RAM+BATTERY", "MBC3+TIMER+BATTERY", "MBC5+RUMBLE", ... ; unknown -> "UNKNOWN" |
| 4   | `romSizeBytes`        | codes $00-$08: `32768 << code`; $52/$53/$54 = 1048576 / 1179648 / 1310720 (canonical sizes, consistent with the fixture generator); other >$08 -> 0 |
| 5   | `ramSizeBytes`        | 0->0, 1->2048, 2->8192, 3->32768, 4->131072, 5->65536 |
| 6   | `headerChecksumValid` | sum($134..$14C) + byte($14D) + 25 == 0 mod 256 |

The checksum convention above matches the fixture generator
(`tests/public/ch16_gameboy_cartridges_mbc/tools/make_roms.py`) and the
boot ROM: corrupting any covered byte breaks it.

## Acceptance

`ch16_01_header_tests` passes: title kept verbatim with $00 padding,
battery table exact for listed + unlisted types, both size-code tables,
and checksum valid on a well-formed header but broken by single-byte
corruption anywhere in the covered range.

Tests synthesize headers in memory; no committed ROM is required.
