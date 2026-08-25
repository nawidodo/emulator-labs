# ch28_gba_ppu — GBA PPU: modes, affine, sprites, compositing

Build a scanline-accurate GBA screen compositor: bitmap modes 3/4/5, text
backgrounds (mode 0/1), affine backgrounds (mode 2), sprites with affine
matrices and priorities, windows, mosaic and blending — all deterministic,
all testable headless with FNV-64 frame hashes.

See `LECTURE.md` first; GBATEK's LCD section is the normative reference.

## Exercises

| Dir | Topic |
|-----|-------|
| `01_bitmap_modes` | BGR555 conversion, mode 3 direct color, mode 4 palette + page flip, mode 5 |
| `02_text_backgrounds` | BGnCNT/screen entries, 4bpp tiles, flips, scroll wrap, priorities |
| `03_affine_transforms` | 8.8 fixed point, reference points, wrapping latched counters |
| `04_sprites` | OAM decode, 1D/2D mapping, affine sprites, sprite-vs-BG priority |
| `05_compositor` | Per-scanline composition, windows, mosaic, blending, headless runner |
| `90_debug` | Seeded bug: sprite priority comparison inverted (see DEBUGGING.md) |
| `91_challenge` | Full-scene golden frame + mGBA graphics suite gate |
| `99_coding_test` | Render a supplied PPU-state snapshot byte-exactly |

## Fixtures

Synthetic only. `.pps` scripts are sequences of `(u32 addr, u16 value)` MMIO
writes applied before rendering (`0xFFFFFFFF` terminator). Snapshots
(`GBASNP1`) dump IO/PAL/VRAM/OAM regions. Both formats are documented in
`99_coding_test/CODING_TEST.md` and under `tests/public/ch28_gba_ppu/`.

## Gate checklist

```text
Exercises       all NN_* skel RED -> student GREEN
Starter         generate.py --targets ch28_gba_ppu builds, tests run RED
Debug           90_debug bug-report.md + fixed priority logic
Challenge       91_challenge golden frame reproduced
Code Test       hidden manifest cases pass on the solution tree
```

## Verification

Recorded from the authoring run (isolated prefix):

```text
VERIFY_PREFIX=/tmp/labs-GBA2 tools/labs/verify_chapter.sh ch28_gba_ppu
[verify] verdict: skel_build=ok solutions=GREEN
```

Golden frames were produced by running the solution-tree runner twice and
confirming identical FNV-64 digests; see
`tests/public/ch28_gba_ppu/provenance.md`.
