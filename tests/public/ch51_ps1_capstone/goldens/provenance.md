# PS1 gate provenance (pad → GTE → MDEC, v013 §34-36)

The first green slice pins the 3 easiest subsystem cases. Goldens are minted
from the canonical PS1 reference runner `tools/labs/ps1_gate/ps1_gate_runner`
composed from verified ch44 (GTE), ch46 (MDEC), and ch48 (SIO pad) headers
when built against `LABS_SOLUTIONS_ROOT` (solutions tree). Fallback
deterministic stubs guarantee the scaffold is runnable even without those
headers, but the pinned hashes come from the solutions build.

## Determinism

Each case was run twice and required byte-identical outputs (`cmp`):

```bash
python3 tools/labs/ps1_gate/gen_ps1_fixtures.py --out-dir .   # ROMs byte-identical on rerun
cmake --build build-solutions --target ps1_gate_runner --parallel 8

# pad
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/pad_txn.bin \
  --input-file tests/hidden/ch51_ps1_capstone/scripts/pad.script \
  --hash-frame /tmp/resp1.bin --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/pad_txn.bin \
  --input-file tests/hidden/ch51_ps1_capstone/scripts/pad.script \
  --hash-frame /tmp/resp2.bin --headless
cmp /tmp/resp1.bin /tmp/resp2.bin   # byte-identical
python3 tools/labs/hash_frame.py /tmp/resp1.bin  # FNV64 93D0B0D1063A168B

# gte
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/gte_vector.bin \
  --hash-frame /tmp/gte1.bin --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/gte_vector.bin \
  --hash-frame /tmp/gte2.bin --headless
cmp /tmp/gte1.bin /tmp/gte2.bin
python3 tools/labs/hash_frame.py /tmp/gte1.bin  # FNV64 DA9769BCBDFB3D04

# mdec
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/mdec_block.bin \
  --hash-frame /tmp/block1.rgba --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/mdec_block.bin \
  --hash-frame /tmp/block2.rgba --headless
cmp /tmp/block1.rgba /tmp/block2.rgba
python3 tools/labs/hash_frame.py /tmp/block1.rgba  # FNV64 2A6E853480FC5307
```

Both runs byte-identical before pinning. FNV-1a 64 is `offset 0xCBF29CE484222325`,
`prime 0x100000001B3`, same as `tools/labs/hash_frame.py`.

## Oracles

| Case                       | Output file | Size | FNV64            | Source ROM |
|----------------------------|-------------|------|------------------|------------|
| `capstone_pad_transaction` | `resp.bin`  | 32   | `93D0B0D1063A168B` | `pad_txn.bin` (256 B, seed `pad_txn`) + `pad.script` (32 B) |
| `capstone_gte_vector`      | `gte.bin`   | 64   | `DA9769BCBDFB3D04` | `gte_vector.bin` (256 B, seed `gte_vector`) |
| `capstone_mdec_block`      | `block.rgba`| 1024 | `2A6E853480FC5307` | `mdec_block.bin` (512 B, seed `mdec_block`) |

`ROM_FNV` values (for the fixture ROMs themselves):

- `pad_txn.bin`      `AD2062407720FDA4`
- `gte_vector.bin`   `972514B1DC44CF4F`
- `mdec_block.bin`   `0C1375988F55AC77`

`pad.script` `975B10A84E1382F1`, `spu.script` `D9104F23F60AD653`.

Build order `pad → GTE → MDEC` (§34-36) — those 3 are green; remaining 7
(`cpu_trace`, `dma_chain`, `timer_irq`, `cd_read`, `spu_stream`, `card_rt`,
`boot_milestones`) stay `expect_file_exists` pending.

## Checkpoint

`ps1_gate_reference.emu_gate` aggregates the 3 FNVs in `EMU_PS1_GATE_V1`
format (see `tools/labs/ps1_gate/EMU_PS1_GATE_V1.md`). Individual
`*.fnv` files (`pad_resp.fnv`, `gte_vector.fnv`, `mdec_block.fnv`) are the
authoritative per-case oracles consumed by the hidden manifest's
`expect_file_hash`.
