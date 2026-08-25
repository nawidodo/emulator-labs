# ch14 fixture provenance

All fixtures are synthetic, generated deterministically by
`tools/make_snapshots.py` (this directory) — no RNG, no wall time, no
ROM content from any commercial product.

```bash
python3 tests/public/ch14_gameboy_ppu1/tools/make_snapshots.py <repo-root>
```

## Files

| file | size | content |
|------|------|---------|
| `fixtures/tile_arrow.bin` | 16 B | one 2bpp tile: rising diagonal + bottom bar |
| `snapshots/plain_tiles.ppu` | 8198 B | v1 state image: LCDC=$91 BGP=$E4 SCY=0 SCX=0 WY=0 WX=0; checker of half/checker tiles |
| `snapshots/scrolled_signed.ppu` | 8198 B | LCDC=$81 ($8800 signed tiles) BGP=$1B SCY=57 SCX=103; map mixes signed -2/0/+2 tile numbers |
| `snapshots/window_scene.ppu` | 8198 B | LCDC=$F1 BGP=$E4 WY=32 WX=47; window on $9C00 with vertical stripes |

## Snapshot v1 layout

`[0..0x1FFF]` VRAM, then bytes: LCDC, BGP, SCY, SCX, WY, WX.

## Listings

Tile plane bytes are emitted by the generator's `tile_bytes()` from
explicit index grids; see the script for each scene's map fill rule.
Regenerating reproduces byte-identical files:

| file | FNV64 |
|------|-------|
| fixtures/tile_arrow.bin | A0C5C6D7A970FC41 |

Golden frame hashes derived from these snapshots live in
`goldens/` and in `tests/hidden/ch14_gameboy_ppu1/manifest.json`; they
were produced by the reference solution runner (run twice, identical).
