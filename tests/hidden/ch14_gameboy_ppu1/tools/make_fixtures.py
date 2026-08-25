#!/usr/bin/env python3
"""Generate HIDDEN fixtures for ch14 (deterministic, synthetic).

  tools/make_fixtures.py <repo-root>

Writes v1 PPU state images into tests/hidden/ch14_gameboy_ppu1/fixtures/.
"""
import sys
from pathlib import Path


def tile_bytes(rows):
    out = bytearray()
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            idx = rows[y][x] & 3
            lo |= ((idx & 1) << (7 - x))
            hi |= (((idx >> 1) & 1) << (7 - x))
        out += bytes([lo, hi])
    return bytes(out)


def pack(vram, lcdc, bgp, scy, scx, wy, wx):
    return bytes(vram) + bytes([lcdc, bgp, scy, scx, wy, wx])


def main(repo_root):
    out = Path(repo_root) / "tests" / "hidden" / "ch14_gameboy_ppu1" / \
        "fixtures"
    out.mkdir(parents=True, exist_ok=True)

    t_noise = tile_bytes(
        [[(x * 3 + y * 5) % 4 for x in range(8)] for y in range(8)])
    t_ring = tile_bytes(
        [[3 if (x in (1, 6) or y in (1, 6)) else 0 for x in range(8)]
         for y in range(8)])

    # h1: deep diagonal scroll over a noise field, inverted palette,
    # $8000 unsigned tiles.
    vram = bytearray(0x2000)
    vram[0x10:0x20] = t_noise
    for i in range(32 * 32):
        vram[0x1800 + i] = 1 if ((i * 7 + i // 32 * 13) % 5) else 0xFE
    (out / "h1_scroll_noise.ppu").write_bytes(pack(vram, 0x91, 0x1B,
                                                   200, 103, 0, 0))

    # h2: $8800 signed tiles, ring tile at -1 ($0FF0), window bottom half.
    vram = bytearray(0x2000)
    vram[0x0FF0:0x1000] = t_ring
    for i in range(32 * 32):
        vram[0x1800 + i] = 0xFF          # tile -1 everywhere
    for row in range(32):
        for col in range(32):
            vram[0x1800 + row * 32 + col] = 0xFF if col % 3 else 0x80
    (out / "h2_signed_nowin.ppu").write_bytes(pack(vram, 0x81, 0xE4,
                                                   40, 12, 32, 7))

    # h3: window covering only the last 21 scanlines, WX near right edge.
    vram = bytearray(0x2000)
    vram[0x10:0x20] = t_noise
    vram[0x20:0x30] = t_ring
    for i in range(32 * 32):
        vram[0x1800 + i] = 1
    for row in range(32):
        for col in range(32):
            vram[0x1C00 + row * 32 + col] = 2
    (out / "h3_window_edge.ppu").write_bytes(pack(vram, 0xF1, 0xE4,
                                                  123, 123, 121, 7 + 139))
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
