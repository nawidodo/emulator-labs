# PS1 gate provenance (pad → GTE → MDEC → CPU → timer → DMA, v013 §34-36)

The first green slice pins the 3 easiest subsystem cases, extended to 6 with CPU trace, timer/IRQ ordering, and DMA chain (build order pad→GTE→MDEC→CPU→timer→DMA). Goldens are minted
from the canonical PS1 reference runner `tools/labs/ps1_gate/ps1_gate_runner`
composed from verified ch44 (GTE), ch46 (MDEC), ch48 (SIO pad), ch38 (R3000A CPU), ch40 (timers/IRQ), and ch43 (DMA) headers
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

# cpu_trace (text trace, cycles-derived length)
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/cpu_smoke.bin \
  --trace /tmp/cpu1.trace --cycles 20000 --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/cpu_smoke.bin \
  --trace /tmp/cpu2.trace --cycles 20000 --headless
cmp /tmp/cpu1.trace /tmp/cpu2.trace   # byte-identical
python3 tools/labs/hash_frame.py /tmp/cpu1.trace  # FNV64 B68B6AFDFB71614E (640051 bytes)

# timer_irq (ordered event log, 256B)
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/irq_order.bin \
  --hash-frame /tmp/evt1.log --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/irq_order.bin \
  --hash-frame /tmp/evt2.log --headless
cmp /tmp/evt1.log /tmp/evt2.log
python3 tools/labs/hash_frame.py /tmp/evt1.log  # FNV64 CD44D0EED2553105 (256 bytes)

# dma_chain (128B state digest)
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/dma_chain.bin \
  --hash-frame /tmp/dma1.state --cycles 100000 --headless
ps1_gate_runner --rom tests/hidden/ch51_ps1_capstone/roms/dma_chain.bin \
  --hash-frame /tmp/dma2.state --cycles 100000 --headless
cmp /tmp/dma1.state /tmp/dma2.state
python3 tools/labs/hash_frame.py /tmp/dma1.state  # FNV64 84DDF8784CD175D9 (128 bytes)
```

Both runs byte-identical before pinning. FNV-1a 64 is `offset 0xCBF29CE484222325`,
`prime 0x100000001B3`, same as `tools/labs/hash_frame.py`.

## Oracles

| Case                       | Output file | Size   | FNV64            | Source ROM |
|----------------------------|-------------|--------|------------------|------------|
| `capstone_pad_transaction` | `resp.bin`  | 32     | `93D0B0D1063A168B` | `pad_txn.bin` (256 B, seed `pad_txn`) + `pad.script` (32 B) |
| `capstone_gte_vector`      | `gte.bin`   | 64     | `DA9769BCBDFB3D04` | `gte_vector.bin` (256 B, seed `gte_vector`) |
| `capstone_mdec_block`      | `block.rgba`| 1024   | `2A6E853480FC5307` | `mdec_block.bin` (512 B, seed `mdec_block`) |
| `capstone_cpu_trace`       | `cpu.trace` | 640051 | `B68B6AFDFB71614E` | `cpu_smoke.bin` (1024 B, seed `cpu_smoke`), cycles 20000 |
| `capstone_timer_irq_order` | `evt.log`   | 256    | `CD44D0EED2553105` | `irq_order.bin` (256 B, seed `irq_order`) |
| `capstone_dma_chain_state` | `dma.state` | 128    | `84DDF8784CD175D9` | `dma_chain.bin` (512 B, seed `dma_chain`), cycles 100000 |

`ROM_FNV` values (for the fixture ROMs themselves):

- `pad_txn.bin`      `AD2062407720FDA4`
- `gte_vector.bin`   `972514B1DC44CF4F`
- `mdec_block.bin`   `0C1375988F55AC77`
- `cpu_smoke.bin`    `0E7BD171EDAA1704`
- `irq_order.bin`    `9DFFE145A21D0225`
- `dma_chain.bin`    `DF8B6BBA443F41AF` (via gen_ps1_fixtures.py; deterministic)

`pad.script` `975B10A84E1382F1`, `spu.script` `D9104F23F60AD653`.

Build order `pad → GTE → MDEC → CPU → timer → DMA` (v013 §34-36) — those 6 are green; remaining 4
(`cd_read`, `spu_stream`, `card_rt`, `boot_milestones`) stay `expect_file_exists` pending.

## Checkpoint

`ps1_gate_reference.emu_gate` aggregates the 6 FNVs in `EMU_PS1_GATE_V1`
format (see `tools/labs/ps1_gate/EMU_PS1_GATE_V1.md`). Individual
`*.fnv` files (`pad_resp.fnv`, `gte_vector.fnv`, `mdec_block.fnv`, `cpu_trace.fnv`, `timer_irq.fnv`, `dma_chain.fnv`) are the
authoritative per-case oracles consumed by the hidden manifest's
`expect_file_hash`.
