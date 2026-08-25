# ch47 Coding Test — unseen ADPCM stream

The hidden suite DMA-loads an **unseen** PSX ADPCM stream into SPU RAM and
plays it through a scripted voice setup with the runner built from
`05_mix` (`ch47_05_spu_runner`). Your implementation must produce the
exact expected PCM bytes; the grade hashes the `--hash-frame` dump.

## Contract

* ROM: raw ADPCM blocks, loaded at SPU RAM byte address `0x1000`.
* Script: same command set documented in `runner_main.cpp`
  (`VOL/PITCH/ADSR/START/MAIN/CDVOL/CD/IRQADDR/IRQON/KEYON/KEYOFF/RENDER`).
* Output: interleaved stereo s16le PCM, hashed with FNV-1a 64.
* Determinism: identical input always produces byte-identical output —
  no time, no RNG.

## What is exercised

Everything from this chapter composes: block decode (ex01), key on/off +
looping (ex02), pitch stepping + interpolation (ex03), envelope (ex04),
mixer, CD-input path and IRQ9 match (ex05).

## Preparing

1. Implement every `TODO(n)` in exercises 01–05.
2. `LABS=ch47_ps1_spu make skels && make build`
3. Rehearse with the public fixture:
   `build/skels/ch47_ps1_spu/05_mix/ch47_05_spu_runner \
      --rom tests/public/ch47_ps1_spu/adpcm/jingle.bin \
      --input-file tests/public/ch47_ps1_spu/scripts/jingle.script \
      --hash-frame /tmp/jingle.pcm --headless`
4. The printed `fnv64` must equal the golden recorded in
   `tests/public/ch47_ps1_spu/goldens/provenance.md`.

The hidden stream uses different shift/filter combinations and a different
script — no pattern matching will save you, only a correct decoder.
