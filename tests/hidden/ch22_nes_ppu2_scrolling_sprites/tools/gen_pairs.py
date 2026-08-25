#!/usr/bin/env python3
"""Hidden frame-pair fixtures for the ch22 coding test.

Builds three pairs (A, B) of 256x240 RGBA8 frames:
  pair_h : B is A scrolled one pixel horizontally   -> shift=h1
  pair_v : B is A scrolled one scanline vertically  -> shift=v1
  pair_o : B is A with scattered corruption         -> shift=other

    python3 gen_pairs.py <out-dir> <base.rgba>
"""
import pathlib
import sys

W, H = 256, 240


def rows(data):
    return [data[y * W * 4:(y + 1) * W * 4] for y in range(H)]


def main():
    outdir = pathlib.Path(sys.argv[1])
    base = pathlib.Path(sys.argv[2]).read_bytes()
    assert len(base) == W * H * 4
    outdir.mkdir(parents=True, exist_ok=True)

    # h1: shift content one pixel to the right; column 0 replicates.
    rs = rows(base)
    shifted = []
    for r in rs:
        first = r[:4]
        shifted.append(r[-4:] + r[:-4] if False else first + r[:-4])
    (outdir / "pair_h_a.rgba").write_bytes(base)
    (outdir / "pair_h_b.rgba").write_bytes(b"".join(shifted))

    # v1: shift down one scanline; row 0 replicates.
    shifted_v = [rs[0]] + rs[:-1]
    (outdir / "pair_v_a.rgba").write_bytes(base)
    (outdir / "pair_v_b.rgba").write_bytes(b"".join(shifted_v))

    # other: deterministic scatter corruption on a copy.
    b = bytearray(base)
    for i in range(0, len(b), 9973):        # prime stride: irregular pixels
        b[i] ^= 0xFF
    (outdir / "pair_o_a.rgba").write_bytes(base)
    (outdir / "pair_o_b.rgba").write_bytes(bytes(b))
    print(f"wrote 6 files under {outdir}")


if __name__ == "__main__":
    main()
