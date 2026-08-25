# Coding test — unseen simplified mapper "MBC-X"

You have never seen this mapper; the spec below is complete. Implement
it in `unseen_mapper.cpp` (the three `TODO` bodies — signatures and the
class skeleton are already provided). The public tests in `main.cpp`
(`TEST(mbcx, ...)`) cover the spec examples; hidden grading replays
op-script runs over a committed MBC-X cart through the chapter runner,
so your implementation must match this document exactly.

## Hardware spec

Cartridge type code **$BE** (header $0147) identifies an "MBC-X" cart.
MBC-X has **no SRAM** and two internal registers:

* `R1` — 3-bit ROM bank select, resets to 1
* `R2` — 1-bit soft open-bus switch, resets to 0

### Writes

| Window      | Effect |
|-------------|--------|
| $2000-$3FFF | `R1 := val & 0x07`; writing 0 selects bank 1 |
| $4000-$5FFF | `R2 := val & 0x01` |
| all others  | ignored |

### Reads

| Window      | Effect |
|-------------|--------|
| $0000-$3FFF | always physical bank 0 |
| $4000-$7FFF | `R2 == 1` ? `$FF` : physical bank `R1`. Offsets beyond the end of the image also read `$FF` |
| $A000-$BFFF | always `$FF` (no RAM chip); writes are silently dropped |

### Spec examples

```text
R 4000   -> 01   ; default R1 = 1
W 2000 07
R 4000   -> 07   ; direct select within the image
W 2000 09        ; masked to 3 bits -> selects bank 1
R 4000   -> 01
W 4000 01        ; soft open bus on
R 4000   -> FF
W 4000 00
R 4000   -> 01
```

## Acceptance

1. `ch16_99_mbcx_tests` green: write decoding, read routing, no-RAM
   behavior, factory dispatch on type $BE (nullptr for other types or
   truncated images).
2. Hidden op-script run over `tests/hidden/ch16_gameboy_cartridges_mbc/roms/h_mbcx.gb`
   matches the reference trace hash via the chapter runner.

## Hints

* Keep the reset values right — the first hidden read happens before
  any write.
* The out-of-image rule matters on the hidden cart: check your offset
  math against the image size, not against the header's claimed size.
