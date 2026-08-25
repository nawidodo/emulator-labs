# 05 — Window layer

The window is a second, unscrollable tilemap overlaid on the BG. Key
hardware behaviors this exercise locks in:

- Active on line LY iff LCD on && LCDC bit5 && `LY >= WY`.
- Screen coverage starts at `WX - 7`; `WX < 7` hides the window.
- The window has its **own internal content line counter** that advances
  only on lines where it actually drew — toggling the window enable bit
  mid-frame therefore skips content rows instead of restarting from WY.

| seq | function           | contract |
|-----|--------------------|----------|
| 1   | `winMapBase`       | LCDC bit 6 selects $9800/$9C00 |
| 2   | `windowActive`     | per-line activity predicate |
| 3   | `renderScanline`   | BG + window overwrite for x >= WX-7 |
| 4   | `renderFrame`      | internal window line bookkeeping |
| 5   | `loadState`        | snapshot loader (same format as ex 04) |

## Acceptance

`ch14_05_window_tests`: region overwrite, WX/WY gating, and the
mid-frame toggle behavior (content row skip) all pass.
