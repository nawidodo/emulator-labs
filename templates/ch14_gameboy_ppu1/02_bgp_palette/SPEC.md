# 02 — BGP palette and shades

A tile color index never reaches the screen directly: the PPU routes it
through the **BG Palette (BGP, $FF47)**, four 2-bit fields selecting a
shade 0..3 (0 lightest, 3 darkest). Games rewrite BGP constantly for
palette-cycling effects, so translation must be per-pixel, not baked in.

| seq | function       | contract |
|-----|----------------|----------|
| 1   | `applyBGP`     | `(bgp >> (index*2)) & 3` |
| 2   | `shadeToRgba`  | fixed grayscale `{255,192,96,0}`, alpha 255 |

## Acceptance

`ch14_02_palette_tests` passes: identity mapping under BGP=$E4,
inverted mapping under $1B, distinct mid-shades.

## Why a fixed grayscale ramp?

Golden frame hashes must be reproducible everywhere. A single fixed
RGBA ramp (documented here and in every chapter-14 renderer) keeps
`--hash-frame` output byte-stable across hosts.
