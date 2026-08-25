#!/usr/bin/env python3
"""Hidden-side synthetic GB cartridge fixtures for ch16.

Run from this directory:
    python3 tools/make_roms.py

Writes into ../roms/:
    h_mbc1.gb   512 KiB (32 banks), type $03, RAM $02 (8 KiB)
    h_mbc3.gb   1 MiB (64 banks), type $0F MBC3+TIMER+BATTERY, RAM $03
    h_mbc5.gb   1 MiB (64 banks), type $19 MBC5+RAM, RAM $02
    h_mbcx.gb   32 KiB (2 banks), type $BE "MBC-X" (unseen coding-test spec)

Same bank pattern and checksum convention as the public generator: every
byte of physical bank k equals (k & 0xFF), except the bank-0 header hole.
"""
from __future__ import annotations

import os

BANK = 0x4000


def build_rom(nbanks: int, cart_type: int, title: str, ram_code: int) -> bytes:
    rom = bytearray()
    for b in range(nbanks):
        rom += bytes([b & 0xFF]) * BANK
    for i in range(0x150):
        rom[i] = 0x00
    t = title.encode("ascii")[:16]
    rom[0x134:0x134 + len(t)] = t
    rom[0x147] = cart_type
    size_code = {2: 0x00, 8: 0x02, 32: 0x04, 64: 0x05}[nbanks]
    rom[0x148] = size_code
    rom[0x149] = ram_code
    s = sum(rom[0x134:0x14D]) + 25
    rom[0x14D] = (-s) & 0xFF
    return bytes(rom)


def main() -> None:
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "roms")
    os.makedirs(out_dir, exist_ok=True)
    specs = [
        ("h_mbc1.gb", build_rom(32, 0x03, "CH16HMBC1", 0x02)),
        ("h_mbc3.gb", build_rom(64, 0x0F, "CH16HMBC3", 0x03)),
        ("h_mbc5.gb", build_rom(64, 0x19, "CH16HMBC5", 0x02)),
        ("h_mbcx.gb", build_rom(8, 0xBE, "CH16HMBCX", 0x00)),
    ]
    for name, rom in specs:
        path = os.path.join(out_dir, name)
        with open(path, "wb") as f:
            f.write(rom)
        print(f"wrote {path} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
