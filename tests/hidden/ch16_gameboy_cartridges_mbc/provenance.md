# provenance — ch16_gameboy_cartridges_mbc (hidden fixtures & goldens)

## Generation

Deterministic synthetic carts; from `tests/hidden/ch16_gameboy_cartridges_mbc/`:

```bash
python3 tools/make_roms.py
```

Outputs (into `roms/`):

| file | banks | header type | RAM code | title |
|------|-------|-------------|----------|-------|
| h_mbc1.gb | 32 (512 KiB) | $03 MBC1+RAM+BATTERY | $02 (8 KiB) | CH16HMBC1 |
| h_mbc3.gb | 64 (1 MiB) | $0F MBC3+TIMER+BATTERY | $03 (32 KiB) | CH16HMBC3 |
| h_mbc5.gb | 64 (1 MiB) | $19 MBC5+RAM | $02 (8 KiB) | CH16HMBC5 |
| h_mbcx.gb | 8 (128 KiB) | $BE "MBC-X" | $00 | CH16HMBCX |

Same pattern rule as the public side (byte = bank index & 0xFF, header
hole in bank 0, valid checksum). The MBC-X image is 8 banks so every
selectable bank number of the 3-bit R1 register holds real patterned
data.

These carts are UNSEEN variants: students practice on the public roms;
hidden cases verify the same mapper contracts against different titles,
sizes, and op sequences.

## Golden trace hashes

Each case was produced by running the reference-solution runner TWICE
with identical args (`cmp` byte-identical both times), from repo root:

```bash
RUNNER=<solution tree>/05_cart_runner/ch16_05_cart_runner
HID=tests/hidden/ch16_gameboy_cartridges_mbc
$RUNNER --rom $HID/roms/h_mbc1.gb --headless \
    --input-file $HID/ops/h_mbc1.ops --trace /tmp/t.log
python3 tools/labs/hash_frame.py /tmp/t.log --fnv-only
```

| manifest case | ops file | fnv64 of trace |
|---------------|----------|----------------|
| mbc1_hidden_ops | ops/h_mbc1.ops | `43A06DD700647991` |
| mbc3_rtc_hidden_ops | ops/h_mbc3.ops | `7E234330CAFB6434` |
| mbc5_hidden_ops | ops/h_mbc5.ops | `0FB5C9B062066D20` |
| mbcx_hidden_ops | ops/h_mbcx.ops | `A180244930BEC0AA` |

Expected read streams:

```text
h_mbc1: R 0200=00 / 4000=05 / 4000=01 / 0200=00 / 4200=01 / A000=5A / A000=FF
h_mbc3: R A000=01 / A000=01 / A000=00 / A000=01
h_mbc5: R 4000=2A / 4000=3F / A010=77 / 4055=09
h_mbcx: R 4000=01 / 4000=07 / 4000=01 / 4000=FF / 4000=01 / 0230=00
```

The optional `hardware_mbc_suite_optional` case references a
student-supplied Mooneye ROM by path only and is honestly skipped when
absent.
