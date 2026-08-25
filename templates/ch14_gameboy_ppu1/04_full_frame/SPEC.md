# 04 — Full frame render + headless runner

Compose exercises 01–03 into a complete 160x144 RGBA frame from a PPU
snapshot image (format documented at the top of `ppu.hpp`: 8 KB VRAM +
LCDC/BGP/SCY/SCX/WY/WX = 8200 bytes).

| seq | function       | contract |
|-----|----------------|----------|
| 1   | `loadState`    | exact-size binary read, reject short/missing files |
| 2   | `renderScanline` | surface fetch -> BGP -> grayscale |
| 3   | `renderFrame`  | 144 lines; LCD off => all white |

## Acceptance

- `ch14_04_frame_tests` passes.
- Runner works end to end:

```bash
ch14_04_frame_runner --rom plain_tiles.ppu --frames 1 \
                     --hash-frame frame.rgba
python3 tools/labs/hash_frame.py frame.rgba --fnv-only
```

The runner implements the mandatory CLI shape; `--cycles`, `--trace`,
`--input-file` are accepted for parity and become meaningful in later GB
chapters. `--frames N` re-renders deterministically (identical output).
