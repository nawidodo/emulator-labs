#!/usr/bin/env python3
"""Deterministic synthetic GB cartridge fixtures for ch16 (public side).

Run from this directory:
    python3 tools/make_roms.py

Writes into ../roms/:
    rom_only.gb      32 KiB, type $00 ROM_ONLY, valid header checksum
    mbc1_512k.gb     512 KiB (32 banks), type $03 MBC1+RAM+BATTERY, RAM $02
    mbc1_2m.gb       2 MiB (128 banks), type $03 MBC1+RAM+BATTERY, RAM $02
    mbc3_timer.gb    1 MiB (64 banks), type $0F MBC3+TIMER+BATTERY, RAM $03
    mbc5_1m.gb       1 MiB (64 banks), type $19 MBC5+RAM, RAM $02
                     (the 9-bit bank behavior is unit-tested; the image is
                     kept small on purpose)

Bank pattern: every byte of physical bank k equals (k & 0xFF), EXCEPT bank 0
offsets $000-$14F which hold the header (title/type/sizes/checksum/logo pad).
A single bus read therefore identifies the mapped bank. Cart RAM initializes
to all $00.

Header checksum convention (matches exercise 01):
    stored($014D) = (-(sum of bytes $134..$14C) - 25) & 0xFF
"""
from __future__ import annotations

import os

BANK = 0x4000


def build_rom(nbanks: int, cart_type: int, title: str, ram_code: int,
              extra: dict[int, int] | None = None) -> bytearray:
    rom = bytearray()
    for b in range(nbanks):
        rom += bytes([b & 0xFF]) * BANK
    # Header hole in bank 0.
    for i in range(0x150):
        rom[i] = 0x00
    t = title.encode("ascii")[:16]
    rom[0x134:0x134 + len(t)] = t
    rom[0x147] = cart_type
    size_code = {2: 0x00, 32: 0x04, 64: 0x05, 128: 0x06}[nbanks]
    rom[0x148] = size_code
    rom[0x149] = ram_code
    for off, val in (extra or {}).items():
        rom[off] = val
    s = sum(rom[0x134:0x14D]) + 25
    rom[0x14D] = (-s) & 0xFF
    return rom


def hexdump_header(rom: bytes) -> str:
    lines = []
    for a in range(0x100, 0x150, 16):
        chunk = rom[a:a + 16]
        hx = " ".join(f"{b:02X}" for b in chunk)
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"  {a:04X}: {hx:<48} {asc}")
    return "\n".join(lines)


def main() -> None:
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "roms")
    os.makedirs(out_dir, exist_ok=True)

    specs = [
        ("rom_only.gb", build_rom(2, 0x00, "CH16ROMONLY", 0x00)),
        ("mbc1_512k.gb", build_rom(32, 0x03, "CH16MBC1512", 0x02)),
        ("mbc1_2m.gb", build_rom(128, 0x03, "CH16MBC12M", 0x02)),
        ("mbc3_timer.gb", build_rom(64, 0x0F, "CH16MBC3RTC", 0x03)),
        ("mbc5_1m.gb", build_rom(64, 0x19, "CH16MBC51M", 0x02)),
    ]
    for name, rom in specs:
        path = os.path.join(out_dir, name)
        with open(path, "wb") as f:
            f.write(rom)
        print(f"wrote {path} ({len(rom)} bytes)")
        print(f"header listing for {name}:")
        print(hexdump_header(rom))


if __name__ == "__main__":
    main()
