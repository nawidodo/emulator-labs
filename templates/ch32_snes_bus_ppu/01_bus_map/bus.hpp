#pragma once
// Exercise 01 — SNES banked memory map.
//
// SNES addresses are 24 bits: `addr = bank << 16 | offset`. Unlike a flat
// 16-bit machine, the bank number participates in decoding, and most regions
// appear at several addresses (mirroring).
//
// The decoder below models the subset of the map this chapter uses:
//
//   region        addresses                              notes
//   ------------  -------------------------------------  --------------------
//   WRAM_DIRECT   $7E0000-$7FFFFF                        128 KiB WRAM, two
//                                                        full 64 KiB banks
//   WRAM_MIRROR   banks $00-$3F (+ $80-$BF mirrors),     aliases WRAM
//                 offsets $0000-$1FFF                    $7E0000-$7E1FFF
//   PPU_REGS      banks $00-$3F (+ $80-$BF mirrors),     the $21xx page repeats
//                 offsets $2100-$21FF                    in every system bank
//   CART_ROM      banks $00-$3F (+ $80-$BF mirrors),     LoROM: 32 KiB pages,
//                 offsets $8000-$FFFF                    see rom_index()
//   OPEN_BUS      anything else                          reads float to $FF,
//                                                        writes are dropped
//
// Access speed: the fastROM bit ($420D) halves ROM wait states for the
// mirrored banks $80-$BF. See cycles_for_access().
//
// Simplifications (documented deviations from full hardware):
//   * Banks $40-$7D and offsets outside the regions above decode to open bus.
//     Real hardware exposes cartridge RAM, DSP ports and expansion chips here.
//   * $00-$3F ROM speed is fixed at 8 master cycles (SlowROM cartridge);
//     only the $80-$BF mirror honours the fastROM bit.

#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace snesbus {

enum class Region : uint8_t {
    WRAM_DIRECT,
    WRAM_MIRROR,
    PPU_REGS,
    CART_ROM,
    OPEN_BUS,
};

// Sentinel returned by wram_offset() for addresses outside WRAM.
inline constexpr uint32_t kNotWram = 0xFFFFFFFFu;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Classify a 24-bit SNES address into one of the five decoded regions.
inline Region region_of(uint32_t addr) {
    const uint32_t bank = addr >> 16;
    const uint32_t off = addr & 0xFFFFu;
    if (bank == 0x7E || bank == 0x7F) {
        return Region::WRAM_DIRECT;
    }
    // System banks and their fastROM mirrors share one decode layout.
    if (bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) {
        if (off < 0x2000) {
            return Region::WRAM_MIRROR;
        }
        if ((off & 0xFF00u) == 0x2100u) {
            return Region::PPU_REGS;
        }
        if (off >= 0x8000) {
            return Region::CART_ROM;
        }
    }
    return Region::OPEN_BUS;
}
//@LABS-STUB
// TODO(1): classify a 24-bit SNES address (bank = addr >> 16) into one of
// the five regions documented in the table at the top of this header.
inline Region region_of(uint32_t) {
    return Region::OPEN_BUS;  // wrong on purpose: every address decodes to open bus
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Map any address to an index into the 128 KiB WRAM array, or kNotWram.
// Direct banks contribute (addr - $7E0000); the $0000-$1FFF mirror window
// aliases the LOW 8 KiB of WRAM only.
inline uint32_t wram_offset(uint32_t addr) {
    switch (region_of(addr)) {
        case Region::WRAM_DIRECT:
            return addr - 0x7E0000u;
        case Region::WRAM_MIRROR:
            return addr & 0x1FFFu;
        default:
            return kNotWram;
    }
}
//@LABS-STUB
// TODO(2): translate a decoded address to a WRAM array index, or return
// kNotWram when the address does not reach WRAM. Remember the mirror window
// only covers the low 8 KiB.
inline uint32_t wram_offset(uint32_t) {
    return kNotWram;  // wrong on purpose: WRAM appears unreachable
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// LoROM page translation: the cartridge exposes 32 KiB pages; offset bit 15
// selects the page half. Bank bits 0-5 pick the page, so banks $00-$3F and
// their $80-$BF mirrors land on identical ROM bytes.
inline size_t rom_index(uint32_t addr) {
    return (static_cast<size_t>(addr >> 16) & 0x3Fu) << 15 |
           (addr & 0x7FFFu);
}
//@LABS-STUB
// TODO(3): compute the LoROM byte index: (bank & $3F) << 15 | (offset & $7FFF).
inline size_t rom_index(uint32_t) {
    return 0;  // wrong on purpose: every read lands on ROM byte 0
}
//@LABS-END

struct Bus {
    std::array<uint8_t, 0x20000> wram{};  // $7E0000-$7FFFFF
    std::array<uint8_t, 256> ppu_regs{};  // $2100-$21FF
    std::vector<uint8_t> rom;             // LoROM image, multiple of 32 KiB
    bool fast_rom = false;                // $420D bit 0

//@LABS-BEGIN 4
//@LABS-SOLUTION
    uint8_t read(uint32_t addr) const {
        const uint32_t w = wram_offset(addr);
        if (w != kNotWram) {
            return wram[w];
        }
        switch (region_of(addr)) {
            case Region::PPU_REGS:
                return ppu_regs[addr & 0xFFu];
            case Region::CART_ROM: {
                const size_t i = rom_index(addr);
                return i < rom.size() ? rom[i] : uint8_t(0xFF);
            }
            default:
                return 0xFF;  // open bus floats high
        }
    }
//@LABS-STUB
    // TODO(4): dispatch a read through wram_offset()/region_of(); ROM reads
    // past the end of the image and open-bus reads return 0xFF.
    uint8_t read(uint32_t) const {
        return 0xFF;  // wrong on purpose: WRAM and PPU ports never answer
    }
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
    void write(uint32_t addr, uint8_t v) {
        const uint32_t w = wram_offset(addr);
        if (w != kNotWram) {
            wram[w] = v;
            return;
        }
        if (region_of(addr) == Region::PPU_REGS) {
            ppu_regs[addr & 0xFFu] = v;
        }
        // Cart ROM and open-bus writes are silently dropped.
    }
//@LABS-STUB
    // TODO(5): route writes to WRAM and PPU registers; ROM/open-bus writes
    // must be dropped without faulting.
    void write(uint32_t, uint8_t) {
        // TODO(5): replace this body.
    }
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
    // Master-cycle cost of one access. FastROM only accelerates the $80-$BF
    // ROM mirror; WRAM, PPU ports and the $00-$3F ROM window stay slow.
    int cycles_for_access(uint32_t addr) const {
        if (region_of(addr) == Region::CART_ROM && (addr >> 16) >= 0x80) {
            return fast_rom ? 6 : 8;
        }
        return 8;
    }
//@LABS-STUB
    // TODO(6): return 6 cycles for $80-$BF ROM accesses when fast_rom is set,
    // 8 cycles otherwise (all WRAM/PPU/$00-$3F ROM accesses).
    int cycles_for_access(uint32_t) const {
        return 8;  // wrong on purpose: never honours the fastROM bit
    }
//@LABS-END
};

}  // namespace snesbus
