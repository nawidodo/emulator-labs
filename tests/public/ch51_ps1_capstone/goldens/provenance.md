# PS1 Capstone Golden Provenance

All ten canonical artifacts were minted by
`tools/labs/ps1_gate/ps1_gate_runner` (built against the verified
`solutions/` tree) from course-owned fixtures in
`tests/hidden/ch51_ps1_capstone/roms/`. Every artifact was produced twice;
both runs were byte-identical (determinism proven, 2 runs each).

Per v014 handoff §11-13 and docs/AUTHORING.md, determinism alone does not
mint a golden. Independent validation per artifact:

| Artifact | Real subsystem path | Independent oracle |
|---|---|---|
| resp.bin | ch48 `sio::DigitalPad` — 6-byte SIO transaction (01 42 select/read) | documented digital-pad serial protocol (PSX-SPX SIO section); response bytes match pad.hpp contract |
| gte.bin | ch44 `Cop2` RTPS over fixture register file; full data-reg file + FLAG serialized | PSX-SPX RTPS equations; FLAG saturation/overflow bits cross-checked against ch44 unit vectors |
| block.rgba | ch46 RLZ decode → IDCT8x8 → YCbCr→RGB15 for all 64 coefficients | PSX-SPX MDEC pipeline order; RGB15 conversion matches ch46 color-conversion unit vector |
| trace.log | ch38 CPU core executing cpu_smoke.bin; EMU_TRACE_V1 format | hand-proved instruction vectors (ALU, branch+delay-slot, load/store) per ch38 tests |
| evt.log | ch40 timer prescaler/target/overflow + IRQ raise/lower through the controller | PSX-SPX timer event semantics; event ordering matches ch40 unit tests |
| dma.state | ch43 DMA channel config, transfer completion, DICR/IRQ state | PSX-SPX DMA transfer semantics + hand-proved transfer vector from ch43 tests |
| out.pcm | ch47 Spu: ADPCM DMA into SPU RAM @0x1000, voice-0 key-on, ADSR envelope, 4096-frame render | known ADPCM decode vector (ch47 adpcm tests) + PSX-SPX ADSR/mix behavior |
| sector.bin | ch45 CdRomController Init→GetStat→Setloc→Pause command path with scheduled latency and IRQ transcript | PSX-SPX CD-ROM command/INT semantics (INT2/INT3/INT5 ordering) |
| card.mcr | ch48 MemCard protocol WRITE pattern block → READ back → export 131072-byte image; checksum + end-flag asserted in transcript | documented memory-card protocol/checksum (PSX-SPX SIO memcard section); roundtrip equality asserted in-transcript |
| boot.log | composition of ALL real capstones above; milestone transcript EMU_PS1_BOOT_V1 with per-device FNV checkpoints | independent validation of each component applies transitively; milestone ORDER is normative per this spec |

## Regeneration

```bash
cmake -S tools/labs/ps1_gate -B /tmp/ps1gate-build \
      -DLABS_SOLUTIONS_ROOT="$PWD/solutions"
cmake --build /tmp/ps1gate-build --parallel 1
RUNNER=/tmp/ps1gate-build/ps1_gate_runner
for case in pad_txn:resp.bin gte_vector:gte.bin mdec_block:block.rgba \
            cpu_smoke:trace.log irq_order:evt.log dma_chain:dma.state \
            spu_stream:out.pcm cd_read:sector.bin card_rt:card.mcr \
            boot_milestones:boot.log; do
  rom="${case%%:*}"; out="${case##*:}"
  "$RUNNER" --rom "tests/hidden/ch51_ps1_capstone/roms/$rom" \
    --headless --hash-frame "tests/public/ch51_ps1_capstone/goldens/$out"
done
# run the loop TWICE and diff — must be byte-identical before committing.
```

## Known limitation

`sector.bin` exercises the command/interrupt path without a disc image
(sector payload comes from the controller's synthetic response). A future
disc-image-backed read will extend this case; the command-path transcript
itself is already pinned.
