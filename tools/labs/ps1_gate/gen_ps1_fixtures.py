#!/usr/bin/env python3
"""gen_ps1_fixtures.py — deterministic ROM/script generator for the PS1 gate.

Generates the 3 pinned fixture ROMs (pad_txn, gte_vector, mdec_block) plus
supporting ROMs/scripts so the hidden manifest's cases have stable inputs.
Rerunning must be byte-identical.

Usage:
  python3 tools/labs/ps1_gate/gen_ps1_fixtures.py [--out-dir .]
  python3 tools/labs/ps1_gate/gen_ps1_fixtures.py --check  # verify rerun identical
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK = 0xFFFFFFFFFFFFFFFF

def fnv(data: bytes) -> int:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK
    return h

def expand(seed: int, n: int) -> bytes:
    out = bytearray()
    x = seed & MASK
    if x == 0:
        x = 0x9E3779B97F4A7C15
    x ^= 0x6A09E667F3BCC908
    x = (x + 0xBB67AE8584CAA73B) & MASK
    for i in range(n):
        x ^= (x >> 12) & MASK
        x ^= (x << 25) & MASK
        x ^= (x >> 27) & MASK
        y = (x * 0x2545F4914F6CDD1D) & MASK
        out.append(y & 0xFF)
        x = (x + 0x9E3779B97F4A7C15 + i) & MASK
    return bytes(out)

# ROM specs: (relative path, size, seed tag)
ROMS = [
    ("tests/hidden/ch51_ps1_capstone/roms/pad_txn.bin", 256, "pad_txn"),
    ("tests/hidden/ch51_ps1_capstone/roms/gte_vector.bin", 256, "gte_vector"),
    ("tests/hidden/ch51_ps1_capstone/roms/mdec_block.bin", 512, "mdec_block"),
    # Additional ROMs for the other 7 cases (pending but need files for CI to not skip)
    ("tests/hidden/ch51_ps1_capstone/roms/cpu_smoke.bin", 1024, "cpu_smoke"),
    ("tests/hidden/ch51_ps1_capstone/roms/dma_chain.bin", 512, "dma_chain"),
    ("tests/hidden/ch51_ps1_capstone/roms/irq_order.bin", 256, "irq_order"),
    ("tests/hidden/ch51_ps1_capstone/roms/cd_read.bin", 2048, "cd_read"),
    ("tests/hidden/ch51_ps1_capstone/roms/spu_stream.bin", 1024, "spu_stream"),
    ("tests/hidden/ch51_ps1_capstone/roms/card_rt.bin", 512, "card_rt"),
    ("tests/hidden/ch51_ps1_capstone/roms/boot_milestones.bin", 2048, "boot_milestones"),
]

SCRIPTS = [
    ("tests/hidden/ch51_ps1_capstone/scripts/pad.script", 32, "pad_script"),
    ("tests/hidden/ch51_ps1_capstone/scripts/spu.script", 32, "spu_script"),
]

def gen_one(size: int, tag: str) -> bytes:
    h = FNV_OFFSET
    for b in tag.encode():
        h ^= b
        h = (h * FNV_PRIME) & MASK
    h ^= 0x5053315F47415445  # "PS1_GATE"
    h = (h * FNV_PRIME) & MASK
    data = expand(h, size)
    # Prefix with a small header so the ROM is recognizable in hex dumps.
    hdr = tag.encode()[:16].ljust(16, b"\x00")
    return hdr + data[len(hdr):] if len(data) >= 16 else data

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--check", action="store_true",
                    help="verify re-run byte-identical (no writes)")
    args = ap.parse_args()
    root = Path(args.out_dir).resolve()

    ok = True
    for rel, size, tag in ROMS + SCRIPTS:
        dest = root / rel
        data = gen_one(size, tag)
        # Ensure size is exact
        if len(data) != size:
            data = (data * ((size // len(data)) + 1))[:size]
        if args.check:
            if not dest.is_file():
                print(f"missing: {rel}")
                ok = False
            elif dest.read_bytes() != data:
                print(f"mismatch: {rel}")
                ok = False
            else:
                print(f"ok {rel} FNV={fnv(data):016X}")
        else:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            print(f"wrote {rel} ({size} bytes) FNV={fnv(data):016X}")

    if args.check:
        return 0 if ok else 1
    # Verify re-read
    for rel, size, tag in ROMS + SCRIPTS:
        dest = root / rel
        expected = gen_one(size, tag)
        if len(expected) != size:
            expected = (expected * ((size // len(expected)) + 1))[:size]
        actual = dest.read_bytes()
        assert actual == expected, f"post-write mismatch {rel}"
    print("gen_ps1_fixtures: all ROMs byte-identical on rerun")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
