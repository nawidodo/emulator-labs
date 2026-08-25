#!/usr/bin/env python3
"""Fixture + program generator for ch17_gameboy_audio_accuracy.

Deterministic (no RNG, no wall time). Produces, relative to the repo root:

  tests/public/ch17_gameboy_audio_accuracy/fixtures/wave_ram_pattern.bin
  tests/public/ch17_gameboy_audio_accuracy/fixtures/noise_lfsr_div0_s0_w15.txt
  tests/public/ch17_gameboy_audio_accuracy/programs/*.apuprog (+ .asm.txt)

.apuprog format: little-endian records of u32 tcycleOffset, u16 regAddr,
u8 value; terminated by a record with regAddr == 0xFFFF.
"""

import struct
import sys
from pathlib import Path

TERMINATOR = (0xFFFFFFFF, 0xFFFF, 0xFF)


def fnv1a(data: bytes) -> str:
    h = 0xCBF29CE484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016X}"


def write_program(path: Path, events, name, description):
    blob = bytearray()
    for t, reg, val in events:
        if not (0xFF10 <= reg <= 0xFF3F):
            raise ValueError(f"{name}: bad register {reg:#06x}")
        blob += struct.pack("<IHB", t, reg, val)
    blob += struct.pack("<IHB", *TERMINATOR)
    path.write_bytes(blob)

    listing = [f"# {path.name} — register write table",
               f"# {description}",
               "# tcycle   reg   value",
               "# ------------------"]
    for t, reg, val in events:
        listing.append(f"  {t:<8d} {reg:#06x}  {val:#04x}")
    listing.append(f"  {'TERM':<8} {0xFFFF:#06x}  --")
    path.with_suffix(".asm.txt").write_text("\n".join(listing) + "\n")
    return fnv1a(blob)


# Distinctive nibble ramp committed for the wave channel exercises.
WAVE_RAM = [0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A,
            0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4,
            0xC3, 0xD2, 0xE1, 0xF0]

FRAME = 70224  # T-cycles per video frame


def square_melody_events():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0x11),
          (0, 0xFF10, 0x1A),   # pace 1, NEGATIVE, slope 2 (descending)
          (0, 0xFF11, 0x80),   # 50% duty
          (0, 0xFF12, 0xF7)]   # volume 15, decay period 7
    notes = [  # (tcycle, freq)
        (0, 1750), (140000, 1795), (280000, 1849), (420000, 1881)]
    for t, f in notes:
        ev += [(t, 0xFF13, f & 0xFF), (t, 0xFF14, 0x80 | (f >> 8))]
    return ev


def wave_arpeggio_events():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0x44),
          (0, 0xFF1A, 0x80), (0, 0xFF1C, 0x20)]  # DAC on, 100%
    for i, b in enumerate(WAVE_RAM):
        ev.append((0, 0xFF30 + i, b))
    steps = [(10000, 100), (80000, 200), (150000, 400), (220000, 800)]
    for t, f in steps:
        ev += [(t, 0xFF1D, f & 0xFF), (t, 0xFF1E, 0x80 | (f >> 8))]
    return ev


def noise_burst_events():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0x88)]
    bursts = [
        (0, 0xC4, 0x35, 0x80),      # vol 12 dec p4, s3/code5, width 15
        (90000, 0xA2, 0x18, 0x80),  # vol 10 dec p2, s1/code0, width 7
        (170000, 0xF1, 0x47, 0x80),  # vol 15 dec p1, s4/code7, width 15
    ]
    for t, nr42, nr43, nr44 in bursts:
        ev += [(t, 0xFF21, nr42), (t, 0xFF22, nr43), (t, 0xFF23, nr44)]
    return ev


def probe_debug_events():
    # Envelope decay + NEGATIVE-mode sweep descent, retriggered once.
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0x11),
          (0, 0xFF10, 0x19),   # pace 1, negate, slope 1
          (0, 0xFF11, 0x80),
          (0, 0xFF12, 0x72),   # volume 7, decay period 2
          (0, 0xFF13, 0xB0), (0, 0xFF14, 0x84 | (1200 >> 8)),
          (180000, 0xFF12, 0x72),
          (180000, 0xFF13, 0xB0), (180000, 0xFF14, 0x84 | (1200 >> 8))]
    return ev


CODING_TEST_EVENTS = [
    (0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0xBF),
    (8, 0xFF11, 0x40), (8, 0xFF12, 0x74), (8, 0xFF13, 0xEE), (8, 0xFF14, 0x86),
    (16, 0xFF16, 0x80), (16, 0xFF17, 0x83), (16, 0xFF18, 0x22), (16, 0xFF19, 0x85),
    (24, 0xFF30, 0x01), (24, 0xFF31, 0x23), (24, 0xFF32, 0x45), (24, 0xFF33, 0x67),
    (24, 0xFF34, 0x89), (24, 0xFF35, 0xAB), (24, 0xFF36, 0xCD), (24, 0xFF37, 0xEF),
    (24, 0xFF38, 0xFE), (24, 0xFF39, 0xDC), (24, 0xFF3A, 0xBA), (24, 0xFF3B, 0x98),
    (24, 0xFF3C, 0x76), (24, 0xFF3D, 0x54), (24, 0xFF3E, 0x32), (24, 0xFF3F, 0x10),
    (24, 0xFF1A, 0x80), (24, 0xFF1C, 0x40), (24, 0xFF1D, 0x00), (24, 0xFF1E, 0x84),
    (32, 0xFF21, 0xF2), (32, 0xFF22, 0x27), (32, 0xFF23, 0x80),
]


def lfsr_bits(width7=False, n=64):
    lfsr = 0x7FFF
    out = []
    for _ in range(n):
        x = (lfsr ^ (lfsr >> 1)) & 1
        lfsr = (lfsr >> 1) | (x << 14)
        if width7:
            lfsr = (lfsr | 0x40) if x else (lfsr & ~0x40)
        out.append((~lfsr) & 1)
    return out


def main():
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
    pub = repo / "tests" / "public" / "ch17_gameboy_audio_accuracy"
    fixdir = pub / "fixtures"
    progdir = pub / "programs"
    fixdir.mkdir(parents=True, exist_ok=True)
    progdir.mkdir(parents=True, exist_ok=True)

    # Wave RAM pattern fixture (mirrored inline by the unit suites).
    pattern = bytes(WAVE_RAM)
    (fixdir / "wave_ram_pattern.bin").write_bytes(pattern)

    # Exact polynomial table: first 64 raw output bits, divisor code 0 /
    # s 0 / width 15 from a fresh 0x7FFF register (cross-checked against
    # the reference solution suite).
    bits = lfsr_bits()
    lines = ["# noise LFSR exact polynomial table",
             "# divisor code 0 (8 T-cycles), s=0, width 15, init 0x7FFF",
             "# raw output bit = (~lfsr & 1) after each step"]
    for row in range(8):
        lines.append("".join(str(b) for b in bits[row * 8:(row + 1) * 8]))
    (fixdir / "noise_lfsr_div0_s0_w15.txt").write_text(
        "\n".join(lines) + "\n")

    programs = [
        ("square_melody", square_melody_events(),
         "pulse 1, four-note melody with positive sweep, decay envelope"),
        ("wave_arpeggio", wave_arpeggio_events(),
         "wave channel nibble-ramp arpeggio, four frequency steps"),
        ("noise_burst", noise_burst_events(),
         "noise channel: three bursts covering width 15 and width 7"),
        ("probe_debug", probe_debug_events(),
         "envelope decay + negative-mode sweep probe (see DEBUGGING.md)"),
    ]
    print("# program            FNV64")
    for name, ev, desc in programs:
        h = write_program(progdir / f"{name}.apuprog", ev, name, desc)
        print(f"{name:20s} {h}")
    # The coding-test configuration ships ONLY in the hidden tree; the
    # identical event list is documented in 99_coding_test/CODING_TEST.md.
    print(f"{'coding_test(hidden)':20s} "
          f"(events mirrored in CODING_TEST.md)")
    print(f"\nwave_ram_pattern.bin     {fnv1a(pattern)}")


if __name__ == "__main__":
    main()
