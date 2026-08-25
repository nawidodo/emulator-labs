#pragma once
#include <cstdint>
#include "bus_tables.hpp"

namespace gba {

// Address routing for the GBA 32-bit bus. Mirroring is part of routing:
// each region exposes its canonical offset inside the region's real size.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Decode an address to its region. ROM chips split by top bits: 08-09 =
// WS0, 0A-0B = WS1, 0C-0D = WS2; SRAM lives at 0E (and its mirror 0F).
inline Region route(uint32_t addr) {
    switch (addr >> 24) {
    case 0x02: return Region::Ewram;
    case 0x03: return Region::Iwram;
    case 0x04: return addr < 0x04001000u ? Region::Io : Region::OpenBus;
    case 0x00: return addr < 0x4000u ? Region::Bios : Region::OpenBus;
    case 0x06: return Region::Vram;
    case 0x05: return Region::Palette;
    case 0x07: return Region::Oam;
    case 0x08: case 0x09: return Region::RomWs0;
    case 0x0A: case 0x0B: return Region::RomWs1;
    case 0x0C: case 0x0D: return Region::RomWs2;
    case 0x0E: case 0x0F: return Region::Sram;
    default:  return Region::OpenBus;
    }
}
//@LABS-STUB
inline Region route(uint32_t addr) {
    // TODO(1): decode the address into its Region per the LECTURE.md map.
    // BIOS/EWRAM/IWRAM/IO/Palette/VRAM/OAM by top byte, ROM split into
    // three waitstate chips (08-09 / 0A-0B / 0C-0D), SRAM at 0E-0F,
    // everything else OpenBus.
    (void)addr;
    return Region::OpenBus;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Canonical VRAM offset after mirroring:
//   1. fold into the 128 K window: off = addr & 0x1FFFF
//   2. the unmapped hole 0x18000-0x1FFFF reflects the BG upper half:
//      subtract 0x10000
//   3. OBJ half (0x10000-0x17FFF) stays DIRECT — no 64 K intra-mirror
//      (that is the discontinuity a naive `& 0xFFFF` gets wrong).
inline uint32_t vram_canonical(uint32_t addr) {
    uint32_t off = addr & 0x1FFFFu;
    if (off >= 0x18000u) off -= 0x10000u;
    return off;
}
//@LABS-STUB
inline uint32_t vram_canonical(uint32_t addr) {
    // TODO(2): apply the two-step VRAM mirror rule above. Beware: folding
    // with & 0xFFFF here is THE seeded trap — it would alias the OBJ bank
    // onto BG memory.
    (void)addr;
    return addr;
}
//@LABS-END

// EWRAM mirrors every 256 K through 0x02FFFFFF; IWRAM every 32 K.
inline uint32_t ewram_canonical(uint32_t addr) { return addr & 0x3FFFFu; }
inline uint32_t iwram_canonical(uint32_t addr) { return addr & 0x7FFFu; }

}  // namespace gba
