#!/usr/bin/env python3
"""Generate the ch32 challenge scene bundles (.sns).

Deterministic: no randomness, no time. Run from anywhere:

    python3 make_scenes.py

Writes baseline.sns and mode7_zoom.sns next to this script.

Scene layout (see ../scene.hpp): "SNESSCN1" magic, u16 version=1,
u16 header_size=80, 4 reserved bytes, 64-byte register block at 0x10,
VRAM (64 KiB) at 0x50, CGRAM (512 B) at 0x10050, OAM low (2048 B) at
0x10250, OAM high (128 B) at 0x10A50. Total 68304 bytes.
"""

import struct

TOTAL = 68304
REGS_OFF = 0x10
VRAM_OFF = 0x50


def build(mode, wrap=False):
    buf = bytearray(TOTAL)
    buf[0:8] = b"SNESSCN1"
    struct.pack_into("<HH", buf, 8, 1, 80)
    r = REGS_OFF

    def vbyte(addr, value):
        w = VRAM_OFF + (addr & ~1)
        if addr & 1:
            buf[w] = value
        else:
            buf[w + 1] = value  # little-endian: byte addr even -> LOW half

    # Wait: file stores words little-endian, so even byte address is the
    # low byte of the word. vbyte writes accordingly:
    def vbyte2(addr, value):
        pos = VRAM_OFF + addr
        word_index = addr >> 1
        base = VRAM_OFF + word_index * 2
        lo = buf[base]
        hi = buf[base + 1]
        if addr & 1:
            hi = value & 0xFF
        else:
            lo = value & 0xFF
        buf[base] = lo
        buf[base + 1] = hi

    if mode == 7:
        struct.pack_into("<HH", buf, r + 0x00, 7, 0x2 if wrap else 0x0)
        # Matrix: slight rotation (~11 deg) + 1.25x zoom around center.
        struct.pack_into("<hhhh", buf, r + 0x1C, 310, 60, -60, 310)
        struct.pack_into("<HHHH", buf, r + 0x24, 128, 112, 160, 140)
        map_base_word = 0x1800
        struct.pack_into("<H", buf, r + 0x06, map_base_word)  # bg1_map_base
        base = map_base_word * 2
        for ty in range(128):
            for tx in range(128):
                n = ((tx // 4 + ty // 4) & 1)
                n = ((tx + ty) % 200 + 1) if n else ((ty + tx * 3) % 200 + 1)
                vbyte2(base + ty * 128 + tx, n)
        for t in range(1, 201):
            for i in range(64):
                vbyte2(t * 64 + i, t % 256)
        # CGRAM ramp + backdrop
        struct.pack_into("<H", buf, 0x10050, 0x3800)
        for i in range(1, 256):
            c = ((i * 5) & 31) | (((i * 3) & 31) << 5) | (((i * 2) & 31) << 10)
            struct.pack_into("<H", buf, 0x10050 + i * 2, c)
    else:
        struct.pack_into("<HH", buf, r + 0x00, 1, 0)
        struct.pack_into("<HHHHHH", buf, r + 0x04,
                         0, 2048,      # bg1 tile/map base
                         512, 3072,    # bg2
                         1024, 4096)   # bg3
        struct.pack_into("<HHHHHH", buf, r + 0x10, 13, 7, 250, 200, 64, 32)
        # BG1 tiles (word 0): four 4bpp tiles
        for t in range(4):
            b = t * 32
            for row in range(8):
                pl = [0, 0, 0, 0]
                for col in range(8):
                    px = ((row ^ col) + t) % 14 + 1
                    for p in range(4):
                        if (px >> p) & 1:
                            pl[p] |= 1 << (7 - col)
                vbyte2(b + row * 2, pl[0])
                vbyte2(b + row * 2 + 1, pl[1])
                vbyte2(b + 16 + row * 2, pl[2])
                vbyte2(b + 17 + row * 2, pl[3])
        # BG2 tiles (word 512): two stripe tiles
        for t in range(2):
            b = 512 * 2 + t * 32
            for row in range(8):
                lo = hi = 0
                for col in range(8):
                    px = 9 if ((col // 2 + t) % 2) else 10
                    if px & 1:
                        lo |= 1 << (7 - col)
                    if (px >> 3) & 1:
                        hi |= 1 << (7 - col)
                vbyte2(b + row * 2, lo)
                vbyte2(b + row * 2 + 1, 0)
                vbyte2(b + 16 + row * 2, 0)
                vbyte2(b + 17 + row * 2, hi)
        # BG3 tile (word 1024): diagonal
        b = 1024 * 2
        for row in range(8):
            p0 = p1 = 0
            for col in range(8):
                px = 3 if row == col else 0
                if px & 1:
                    p0 |= 1 << (7 - col)
                if (px >> 1) & 1:
                    p1 |= 1 << (7 - col)
            vbyte2(b + row * 2, p0)
            vbyte2(b + row * 2 + 1, p1)
        # Maps
        for i in range(1024):
            tx, ty = i & 31, i >> 5
            struct.pack_into("<H", buf, VRAM_OFF + (2048 + i) * 2,
                             0x2000 | (2 << 10) | ((tx * ty) & 3))
            struct.pack_into("<H", buf, VRAM_OFF + (3072 + i) * 2,
                             (((tx + ty) & 4) << 10) | 512 + ((tx + ty) & 1))
            struct.pack_into("<H", buf, VRAM_OFF + (4096 + i) * 2,
                             (8 << 10) if ((tx & 4) and (ty & 4)) else 0)
        # CGRAM
        struct.pack_into("<H", buf, 0x10050, 0x294A)
        for p in range(8):
            for c in range(1, 16):
                e = ((c * 2) & 31) | (((p * 4 + 8) & 31) << 5) | (0x15 << 10)
                struct.pack_into("<H", buf, 0x10050 + (p * 16 + c) * 2, e)
        for p in range(8):
            for j, val in enumerate((0x0000, 0x0366, 0x06C9, 0x7BDE)):
                struct.pack_into("<H", buf, 0x10050 + (32 + p * 4 + j) * 2,
                                 val)
        # Window [48,143], mask keeps layers 1-3 (bit2..bit5), math inside on
        buf[r + 0x2C] = 48
        buf[r + 0x2D] = 143
        buf[r + 0x2E] = 0x01 | (0b1111 << 2) | 0x40  # enable+mask+cmath
        buf[r + 0x2F] = 0x04 | 0x02                  # on + half, add
    return bytes(buf)


def main():
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(out, "baseline.sns"), "wb") as f:
        f.write(build(1))
    with open(os.path.join(out, "mode7_zoom.sns"), "wb") as f:
        f.write(build(7, wrap=False))
    print("wrote baseline.sns and mode7_zoom.sns")


if __name__ == "__main__":
    main()
