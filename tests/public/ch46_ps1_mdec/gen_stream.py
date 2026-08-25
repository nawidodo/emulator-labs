#!/usr/bin/env python3
"""Deterministic MDEC compressed-stream generator (chapter 46 fixtures).

Emits the byte format documented in templates/ch46_ps1_mdec/91_challenge/
mdec_core.hpp: a sequence of macroblocks (Y0 Y1 Y2 Y3 Cb Cr), each block
prefixed by a big-endian u16 unit count followed by that many big-endian
u16 RLZ units (header, run/level pairs, FE00 terminator).

Usage: gen_stream.py OUT_FILE [seed] [n_macroblocks]
"""
import math
import struct
import sys

ZZ = [
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
]


def encode_block(coeffs):
    """coeffs: dict natural_index -> signed value. Flat Q tables (16),
    scale 16: level = value / (16*16/16) = value/16 for AC, *8/... for DC.
    We pick values that are exactly representable."""
    units = [0x0010]  # header: luma table, scale 16
    zz_pos = 0
    run = 0
    entries = sorted((ZZ.index(nat), val)
                     for nat, val in coeffs.items() if val != 0)
    for pos, _val in entries:
        pass
    # rebuild with zig-zag positions and levels computed by caller
    return units


def encode_block_units(entries, scale=16, chroma=False):
    """entries: list of (zigzag_pos, signed_level) sorted ascending."""
    hdr = (0x8000 if chroma else 0) | (scale & 0x7FFF)
    units = [hdr]
    zz = 0
    for pos, lvl in entries:
        run = pos - zz
        zz = pos
        code = lvl & 0x3FF
        assert -512 <= lvl <= 511
        units.append((run << 10) | code)
        zz += 0  # position consumed
    units.append(0xFE00)
    return units


def macroblock(dc_luma, dc_cb=4, dc_cr=4, extra_ac=None):
    """DC-only blocks (plus optional extras) -> six encoded blocks."""
    blocks = []
    specs = []
    for i in range(4):
        lvl = dc_luma[i]
        ent = [(0, lvl)]
        if extra_ac:
            ent += extra_ac[i]
            ent.sort()
        specs.append((ent, False))
    specs.append(([(0, dc_cb)], True))
    specs.append(([(0, dc_cr)], True))
    out = b""
    for entries, chroma in specs:
        units = encode_block_units(entries, 16, chroma)
        out += struct.pack(">H", len(units))
        out += struct.pack(">%dH" % len(units), *units)
    return out


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "stream.bin"
    seed = int(sys.argv[2], 0) if len(sys.argv) > 2 else 1
    n_mb = int(sys.argv[3]) if len(sys.argv) > 3 else 4

    data = b""
    for mb in range(n_mb):
        # Deterministic pseudo-random flat luma per Y block.
        dcs = []
        for blk in range(4):
            v = ((seed + mb * 7 + blk * 13) % 24) + 1   # 1..24
            dcs.append(v & 0x3FF)
        cb = ((seed + mb * 5) % 8) + 1
        cr = ((seed + mb * 11) % 8) + 1
        data += macroblock(dcs, cb, cr)

    while len(data) % 4:
        data += b"\x00\x00"  # zero-length block prefix stops the decoder
    open(out_path, "wb").write(data)
    print(f"wrote {out_path}: {n_mb} macroblocks, {len(data)} bytes")


if __name__ == "__main__":
    main()
