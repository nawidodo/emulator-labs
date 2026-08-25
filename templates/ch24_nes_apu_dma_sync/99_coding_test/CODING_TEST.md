# Coding test — ch24: repair the drifting machine

A "simplified" build of the Chapter 24 machine advances its PPU by only
**2 dots per CPU cycle** instead of the hardware ratio of **3**. Nothing
crashes. Instead, everything slowly drifts:

- raster splits land at the wrong scanline and slide further every frame;
- frame boundaries arrive after ~134k CPU cycles instead of ~29.8k;
- frame and audio hashes wander away from the reference contract below.

Your job: find the ratio constant in `machine.hpp`
(`nes24sync::kPpuDotsPerCpu`) and restore the hardware relationship —
one CPU cycle drives exactly three PPU dots, because NTSC divides the
master clock by 12 for the CPU and by 4 for the PPU.

## The tool you verify with

```
ch24_99_drift_runner --rom SCRIPT --frames N [--trace F] [--hash-frame F]
                     [--audio-out F]
```

(same op-script grammar as the Chapter 24 runner: `pal`, `chrpat`, `nt`,
`wr`, `frame`).

## Expected-hash contract (public rehearsal)

Run the drift runner over the public fixture
`tests/public/ch24_nes_apu_dma_sync/fixtures/drift_probe.txt` with 60
frames. With a CORRECT ratio you must reproduce:

| Output | Value |
|---|---|
| frame hash (`fnv64`) | see `tests/public/ch24_nes_apu_dma_sync/goldens/provenance.md` |
| audio hash (`audio_fnv64`) | see same provenance |

With the broken ratio you will NOT — that is the whole test.

## Grading

The hidden manifest runs your repaired machine over an UNSEEN script and
compares both hashes byte-exactly against reference values generated from
the same fixture.
