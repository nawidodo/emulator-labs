# Provenance — ch26_gba_thumb_pipeline_exceptions public fixtures & goldens

All fixtures are synthetic; no commercial or downloaded ROM data is used.

## roms/

- `interleave.bin` (+ `.asm.txt`): ARM<->Thumb interleave round trip for
  the 03_mode_switch exercise. ARM words at 0x00, Thumb halfwords at 0x40.
  Final state: r0=8, r1=18, r2=10, parked in ARM at 0x18 with T cleared.
- `callret.bin` (+ `.asm.txt`): Thumb PUSH/POP call/return discipline for
  the 91_challenge acceptance criteria (helper clobbers caller registers,
  stack save/restore proves preservation). Final state: r0=7, r1=9, r2=16,
  sp=0x800.

## traces/ and goldens/

- `traces/interleave.log`: canonical trace from the reference solution:
  `ch26_switch_runner --rom roms/interleave.bin --headless --cycles 21
  --trace ...`
- `traces/callret.log` and `goldens/callret.dump`: trace + final register
  dump via `ch26_challenge_runner --cycles 20`.

Every artifact was generated TWICE by the reference solution runners and
byte-compared before committing; all pairs were identical. Hidden manifest
hashes (`tests/hidden/ch26_gba_thumb_pipeline_exceptions/manifest.json`)
were computed from those same outputs (FNV-1a 64).

## Hidden fixtures

`tests/hidden/ch26_gba_thumb_pipeline_exceptions/roms/coding_trace.bin`
(+ `.asm.txt`) exercises imm8 flags, a conditional backward branch and a
literal-pool load; its golden trace/dump hashes live only in the hidden
manifest.
