# CHALLENGE — 91_challenge: textured quad to a golden VRAM hash

## Task

Assemble a GP0 command stream that makes the reference GPU render a
**textured quadrilateral** — two flat-shaded textured triangles
(`GP0(24h)`, vertices split as 1-2-3 and 2-3-4 per the hardware's internal
quad handling) plus textured rectangles (`GP0(74h)`/`GP0(75h)`), with all
texture data delivered through CPU→VRAM transfers (`GP0(A0h)`) inside the
same stream.

The committed fixture `challenge_quad.bin` (and its embedded twin in
`fixture_quad.hpp`) is the acceptance artifact:

```bash
./ch42_91_challenge_runner \
    --rom tests/public/ch42_ps1_gpu2_texturing_raster/fixtures/challenge_quad.bin \
    --hash-frame /tmp/quad.hash
# -> fnv64=<golden>   must equal goldens/challenge_quad.hash
```

## Acceptance criteria

1. `ch42_91_challenge_tests` passes: the embedded stream reproduces
   `ch42fixture::kQuadVramHash` over the full VRAM dump, twice in a row.
2. The runner produces `goldens/challenge_quad.hash` byte-for-byte from the
   public fixture.
3. Your own variant (optional): change any vertex, texel or shade in your
   copy of the generator and confirm the hash moves — proves you understand
   which words carry geometry vs. attributes.

## What the exercise teaches

- Word layout of textured polygons: colour word, vertex/texcoord pairing,
  CLUT attribute on texcoord 1, Texpage attribute on texcoord 2.
- Quad splitting into two triangles and why the shared diagonal must agree
  under the top-left fill rule.
- Uploading 4bpp texture + CLUT rows with word-aligned row padding.
- Mixing modulate (`74h`) and decal (`75h`) rectangles against the same
  texture page.
