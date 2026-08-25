# provenance — tests/public/ch42_ps1_gpu2_texturing_raster

All fixtures are synthetic GP0 command streams assembled by a throwaway
Python generator (word-level assembler, ~200 lines, not committed per the
fixture policy). Each `.asm.txt` lists every u32 word with its byte offset;
the sections below describe what each stream exercises.

## fixtures/pages_blit.bin

1. E3/E4/E5/E6 reset; A0h uploads:
   - 16-entry 4bpp CLUT → (0,240), entry i = `0x111*(i+1)`
   - 16×16 4bpp texture (packed 4 nibbles/halfword) → page (0,0),
     texel(x,y) = `(x+y) & 15`
   - 256-entry 8bpp CLUT → (0,241), entry i = `0x0101*(i+1)`
   - 32×32 8bpp texture (packed 2 bytes/halfword) → page (64,0)
     (**odd** page field → lane-mirror quirk), texel = `x ^ y`
   - 17×16 15bpp texture (odd width → row padding exercised) → page (128,0)
2. Three raw-texture (`65h`) rectangles: 15bpp direct blit, 4bpp CLUT blit,
   32×32 8bpp odd-page blit — each preceded by its own GP0(E1h).

Golden: `goldens/pages_blit.hash` = FNV-1a-64 of the runner's
`--hash-frame` payload. Trace golden `traces/pages_blit.trace` from the same
run (`--trace`, one `pc=/op=/cyc=` line per command start).

Regenerating:

```
ch42_01_texture_pages_runner --rom fixtures/pages_blit.bin \
    --hash-frame goldens/pages_blit.hash --trace traces/pages_blit.trace
```

## fixtures/blend_span.bin

Grey fill backdrop; 32×32 15bpp texture at page (320,0) containing `0000h`
transparent holes. Then:

- one modulated rect and four semi-transparent rects (`66h`) cycling all
  four semi equations over identical texture data,
- a tight drawing area (E3/E4) with a straddling rect (exact inclusive clip),
- drawing offset (E5) shifting a rect across that clip boundary,
- mask round trip: Set-mask stamps bit15, Check-mask re-draw over it,
- a dithered textured triangle (24h, dither flag set).

Golden: `goldens/blend_span.hash`.

## fixtures/challenge_quad.bin

Dark blue fill; 16-colour CLUT at (0,480); 64×64 4bpp texture packed into a
16-halfword-wide strip at page (0,256); two flat-shaded textured triangles
(`24h`) forming a quadrilateral (vertices split 1-2-3 / 2-3-4); an 8×8
modulated rect (`74h`) and an 8×8 decal rect (`75h`).
Golden: `goldens/challenge_quad.hash`; also embedded verbatim in
`templates/ch42_ps1_gpu2_texturing_raster/91_challenge/fixture_quad.hpp`
with the same hash as `kQuadVramHash`.

## fixtures/debug_probe.bin

Odd-page 4bpp rect clipped so its bottom-right corner lands exactly on the
drawing-area corner (exercises both seeded 90_debug bugs in one frame) plus
an even-page control rect.

## Golden generation

All hashes were produced by the reference solution binaries (skeleton
generator, `--mode solution`) run **twice per fixture** with byte-compare in
between; only matching digests were committed. Example generating command:

```
ch42_91_challenge_runner --rom fixtures/challenge_quad.bin \
    --hash-frame goldens/challenge_quad.hash
```

The digest input is the full 1024×512 halfword VRAM dump serialized
little-endian; the payload format is `fnv64=<16 uppercase hex>\n`.
