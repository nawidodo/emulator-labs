# 02 — MBC1

Register semantics implemented here (identical in the runner's copy):

| seq | function    | contract |
|-----|-------------|----------|
| 1   | `writeReg`  | $0000-$1FFF: `ramEnable = (val & 0x0F) == 0x0A` (any other value disables). $2000-$3FFF: `bank1 = val & 0x1F`, 0 -> 1. $4000-$5FFF: `bank2 = val & 0x03`. $6000-$7FFF: `mode = val & 1`. |
| 2   | `readRom` (via `physicalBankHi`) | $4000-$7FFF: physical bank `((bank2<<5) \| bank1) % nbanks`. $0000-$3FFF: mode 0 -> bank 0; mode 1 -> `(bank2<<5) % nbanks`. |
| 3   | `readRam`   | disabled or absent RAM -> $FF; RAM bank = mode 1 ? `bank2 & 3` : 0; offset masked by RAM size |
| 4   | `writeRam`  | mirror of read-side gating/banking; silent while disabled |

`CartridgeController::makeMapper` dispatches header types $01-$03 to
Mbc1 and returns nullptr otherwise.

## Acceptance

`ch16_02_mbc1_tests` passes over in-memory patterned images where every
byte of bank *k* equals *k*:

* bank identity through the high half, including the 2 MiB / 128-bank
  image that needs all of bank2;
* zero-writes-to-bank-1 quirk and 5-bit masking;
* out-of-range selects wrap modulo ROM size;
* mode 0 pins the low half to bank 0, mode 1 shows `(bank2<<5)` there;
* RAM enable gate, absent-RAM open bus ($FF), mode-1 RAM banking.

## Why modulo wrap?

Real MBC silicon compares the requested bank against wired address
lines; unconnected lines read back 0. The observable behavior is a
modulo mask by ROM size — never open bus — so emulators implement it
exactly that way.
