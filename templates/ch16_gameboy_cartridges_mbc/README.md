# ch16_gameboy_cartridges_mbc — cartridges, MBC1/3/5, battery SRAM

Models the cartridge side of the DMG as pure, headless mapper strategies
(no CPU needed): header parsing, MBC1 dual-mode banking, MBC3 with a
deterministic injected-tick RTC, MBC5 nine-bit banks + battery save,
and an integrated op-script runner.

```text
01_header_parse    title/type/size codes/header checksum
02_mbc1            four register windows, banking MODE, RAM gating
03_mbc3            7-bit banks, RTC registers, latch handshake
04_mbc5_battery    9-bit ROM banks, SRAM .sav round-trip
05_cart_runner     CartridgeController factory + headless runner CLI
90_debug           three seeded mapper defects (bug-report required)
91_challenge       multi-bank traversal scripts vs golden read streams
99_coding_test     unseen-spec coding test: simplified "MBC-X"
```

Design: every mapper answers one strategy interface (`Mapper`), built by
`CartridgeController::makeMapper(rom, size)` dispatching on header type.
Exercise headers are deliberately self-contained duplicates so each
exercise compiles alone.

## Gate checklist

- [ ] exercises: all suites green (`LABS=ch16_gameboy_cartridges_mbc make skels && make test`)
- [ ] starter: `ch16_05_cart_runner --help` works; op scripts replay
- [ ] debug: 90_debug fixed AND `bug-report.md` written
- [ ] challenge: three golden stdout blocks match (see 91_challenge/CHALLENGE.md)
- [ ] coding_test: `make grade GRADE_TARGETS=ch16_gameboy_cartridges_mbc` exits 0

## Fixtures

All carts are synthetic and deterministic; committed under
`tests/public/ch16_gameboy_cartridges_mbc/roms/` with generator +
provenance next to them. Pattern rule: **every byte of physical bank k
equals k & 0xFF** except the bank-0 header hole ($000-$14F), so any bus
read identifies its mapped bank. Cart RAM powers up all $00. Hidden-side
variants (`h_*.gb`, including an 8-bank MBC-X image) live under
`tests/hidden/ch16_gameboy_cartridges_mbc/roms/`.

## Runner CLI

Mandatory shape: `--rom PATH --headless --cycles N --frames N --trace
FILE --hash-frame FILE --input-file FILE --help`. `--cycles/--frames`
are accepted no-ops here (no CPU/PPU loop in this chapter).

### Extensions (documented per the common contract)

`--input-file` takes a cartridge **op script**, one op per line, `#`
comments allowed:

| op | meaning |
|----|---------|
| `W <hexaddr> <hexval>` | bus write: $0000-$7FFF hits mapper registers, $A000-$BFFF hits cart RAM / RTC |
| `R <hexaddr>` | bus read ($0000-$7FFF or $A000-$BFFF); echoes `R <hexaddr>=<hexval>` (uppercase hex) to **stdout** |
| `T <dec-cycles>` | advance the injected-tick RTC by N T-cycles (MBC3 only; other mappers ignore it) |

`--trace FILE` receives exactly the stdout R-line stream;
`--hash-frame FILE` writes those same bytes, so golden hashes are
computed with `python3 tools/labs/hash_frame.py FILE --fnv-only`.
Hidden manifest cases reference trace FNV-1a-64 hashes.

## Mapper coverage notes

* MBC1 mode 1 maps the low half to physical bank `(bank2 << 5)` mod
  ROM size and picks RAM bank `bank2 & 3`; mode 0 pins both to 0.
* Out-of-range bank selects wrap modulo the real image size everywhere
  (hardware masks unwired address lines; never open bus).
* The MBC5 9-bit select is unit-tested through seams on small images;
  no multi-megabyte fixture is committed for it.
* The checksum convention (sum + stored byte + 25 == 0 mod 256, i.e.
  stored = (-(sum+25)) & 0xFF) matches boot-ROM behavior and the
  fixture generator.

## Verification (recorded)

```text
VERIFY_PREFIX=/tmp/labs-Ch16 tools/labs/verify_chapter.sh ch16_gameboy_cartridges_mbc
  SKEL:      build OK; ctest RED (7/8 failing as expected)
  SOLUTIONS: GREEN — 100% tests passed out of 8
grade.py (solution binaries vs tests/hidden/ch16_gameboy_cartridges_mbc):
  4/4 hidden cases PASS (+1 optional requires_rom skip, mooneye absent):
  mbc1_hidden_ops, mbc3_rtc_hidden_ops, mbc5_hidden_ops, mbcx_hidden_ops
Goldens generated twice from the reference solution runner, byte-identical:
  mbc1_hidden_ops   43A06DD700647991
  mbc3_rtc_hidden   7E234330CAFB6434
  mbc5_hidden_ops   0FB5C9B062066D20
  mbcx_hidden_ops   A180244930BEC0AA
Challenge traces: c1 256EC39539CE01A8 / c2 0D88972BE8E11F32 /
c3 0FB5C9B062066D20 (see tests/public/ch16_gameboy_cartridges_mbc/)
```
