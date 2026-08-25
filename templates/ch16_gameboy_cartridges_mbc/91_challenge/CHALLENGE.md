# Challenge — multi-bank traversal with the cartridge runner

No new code: this challenge is about *driving* the mappers you built.
Using the committed synthetic carts and the op-script extension, replay
the three traversal scripts and match the golden read streams exactly.

```bash
RUNNER=path/to/ch16_05_cart_runner
PUB=tests/public/ch16_gameboy_cartridges_mbc

$RUNNER --rom $PUB/roms/mbc1_512k.gb --headless \
        --input-file $PUB/ops/c1_mbc1_banks.ops
$RUNNER --rom $PUB/roms/mbc3_timer.gb --headless \
        --input-file $PUB/ops/c2_mbc3_rtc.ops
$RUNNER --rom $PUB/roms/mbc5_1m.gb --headless \
        --input-file $PUB/ops/c3_mbc5_high.ops
```

## Acceptance — expected stdout, byte for byte

### c1_mbc1_banks.ops (MBC1, 32 banks)

```text
R 0200=00
R 4000=05
R 4321=05
R 4000=01
R 4200=01
R A123=9C
```

Why each line is what it is: bank select $05 shows pattern byte $05 at
$4000; writing $00 wraps to bank 1; mode 1 with bank2=2 maps the low
half to physical bank (2<<5) % 32 = 0 while the high half becomes bank
(64+1) % 32 = 1; enabled SRAM echoes the write.

### c2_mbc3_rtc.ops (MBC3 RTC, injected ticks)

```text
R 4000=01
R 4123=03
R A000=01
R A000=01
R A000=00
R A000=01
```

One tick of exactly 4194304 cycles sets live seconds to 1. After the
latch, reads come from shadows frozen at t=1 s even as a further 59 s
are injected; re-latching captures minutes=1, seconds=0.

### c3_mbc5_high.ops (MBC5 nine-bit banks)

```text
R 4000=2A
R 4000=3F
R A010=77
R 4055=09
```

Selects wrap mod 64 banks on this 1 MiB image: 511 % 64 = 63 ($3F),
and clearing bit 9 then writing $09 lands on bank 9.

## Golden trace hashes

FNV-1a-64 over `--trace` output (see provenance.md for the commands):

| script | fnv64 |
|--------|-------|
| c1_mbc1_banks.ops | `256EC39539CE01A8` |
| c2_mbc3_rtc.ops | `0D88972BE8E11F32` |
| c3_mbc5_high.ops | `0FB5C9B062066D20` |

Compute your own with:

```bash
python3 tools/labs/hash_frame.py TRACE_FILE --fnv-only
```
