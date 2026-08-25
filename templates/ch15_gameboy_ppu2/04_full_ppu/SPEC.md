# 04 — Full PPU and the chapter runner

Complete scanline pipeline over a snapshot v2 image:

```text
1. BG fetch        scrolled surface, SCX/SCY wrap mod 256,
                   $8000/$8800 addressing, BGP shading
2. window pass     x >= WX-7, unscrolled map, INTERNAL content-line
                   counter (advances only on lines where it drew)
3. sprite pass     first <=10 covering OAM entries (x==0 skipped
                   pre-limit), per-column winner, transparency at index 0,
                   0x80 priority vs BG color INDEX, flips, 8x16 pairing,
                   OBP0/OBP1
```

| seq | function | contract |
|-----|----------|----------|
| 1 | `loadState` | exact-size read of the v2 image into VRAM+OAM+registers |
| 2 | `renderBgWindowScanline` | BG indices + BGP shades, then window overwrite using `windowLine` |
| 3 | `compositeScanline` | exercise-01 sprite rules onto precomputed BG/window layers |
| 4 | `renderFrame` | 144 lines + window counter; honors the per-line enable mask |

## Runner

`ch15_04_fullppu_runner` accepts the mandatory CLI shape plus two
extensions:

```text
--rom SNAPSHOT.ppu2          loads the v2 state image
--frames N                   repeat render (deterministic; output identical)
--hash-frame OUT.rgba        raw RGBA8 dump for hash_frame.py
--trace OUT.log              mode-transition log of the first frame
                             ("ly=<n> dot=<n> mode=<m>\n" per change)
--window-off-lines A:B       disable the window for screen lines [A,B);
                             skipped lines do NOT advance the window's
                             internal content counter
--headless --cycles N --input-file FILE    accepted, no-op here
```

## Acceptance

`ch15_04_fullppu_tests`: scroll/BGP fetch, BG-disabled behavior, window
gating, LCD-off white frame, determinism, sprite overlay, and the
window-toggle skip behavior. The runner help test checks the CLI shape.

Pan Docs: "Background", "Window", "OAM", "LCD timing".
