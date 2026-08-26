# EMU_PS1_GATE_V1 — PS1 whole-machine checkpoint format (canonical reference)

Version 1 checkpoint payload produced by the canonical PS1 reference gate
(`tools/labs/ps1_gate/ps1_gate_runner`) and consumed by the grader for
`expect_file_hash`-style pinning of the ch51 capstone integration cases.

Direct analogue of `tools/labs/nes_gate/EMU_GATE_V1.md` with PS1-specific
subsystem hashes (GTE/MDEC/PAD) in addition to the common frame hash.

## Layout

One checkpoint per run, written to the `--gate` output file. Plain text,
UTF-8, LF line endings, `KEY=VALUE` lines; order is not significant;
unknown keys MUST be ignored by consumers; a missing required key makes
the checkpoint INVALID.

```
EMU_PS1_GATE_V1
ROM_FNV=XXXXXXXXXXXXXXXX
GTE_FNV=XXXXXXXXXXXXXXXX
MDEC_FNV=XXXXXXXXXXXXXXXX
PAD_FNV=XXXXXXXXXXXXXXXX
FRAME_FNV=XXXXXXXXXXXXXXXX
REPLAY_FNV=XXXXXXXXXXXXXXXX
```

## Fields

| Key         | Meaning                                                          |
|-------------|------------------------------------------------------------------|
| `ROM_FNV`   | FNV-1a 64 (uppercase hex `%016X`) over the raw ROM bytes          |
| `GTE_FNV`   | FNV-1a 64 over the GTE result payload (`gte.bin` content)         |
| `MDEC_FNV`  | FNV-1a 64 over the MDEC decoded RGBA (`block.rgba` content)       |
| `PAD_FNV`   | FNV-1a 64 over the SIO pad response (`resp.bin` content)          |
| `FRAME_FNV` | FNV-1a 64 over the primary `--hash-frame` output for the case     |
| `REPLAY_FNV`| FNV-1a 64 over a second deterministic run with identical input;   |
|             | must equal `FRAME_FNV` when replay self-check is enabled          |

Additional keys (`CPU_PC`, `CPU_CYC`, `CYCLES`, etc.) MAY be present for
debugging but are not required for the gate.

FNV-1a 64: offset basis `0xCBF29CE484222325`, prime `0x100000001B3`,
byte-at-a-time, same constants as `tools/labs/hash_frame.py` and the
NES gate's `fnv1a64`.

## Build order

v013 §34-36 mandates the subsystem wiring order `pad → GTE → MDEC` for
the first green slice. The scaffold pins exactly those 3 cases
(`capstone_pad_transaction`, `capstone_gte_vector`, `capstone_mdec_block`);
the remaining 7 cases stay `expect_file_exists` until their subsystems
(DMA/CD/SPU/timers) land.

## Provenance

- Written by `tools/labs/ps1_gate/ps1_gate_runner`, composed from the
  verified ch44 (GTE), ch46 (MDEC), and ch48 (SIO pad) components when
  built against `LABS_SOLUTIONS_ROOT`.
- Goldens minted by running the reference twice and requiring byte-identical
  outputs (see `tests/public/ch51_ps1_capstone/goldens/provenance.md`).
- Checkpoint and per-case `*.fnv` hashes generated with `hash_frame.py`.

## CLI

The runner accepts the hidden manifest's args and exits 0 on success:

```
ps1_gate_runner --rom <rom> --hash-frame <out> [--input-file <script>]
                [--frames N] [--cycles N] [--trace <file>] [--gate <file>]
                [--headless]
```

Unknown flags are ignored (forward-compatible). `--headless` is always
accepted. Determinism: same `--rom` + `--input-file` bytes yield
byte-identical `--hash-frame` output (verified by double-run + `cmp`).
