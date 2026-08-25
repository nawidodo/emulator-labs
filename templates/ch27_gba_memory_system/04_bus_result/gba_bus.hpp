#pragma once
#include <cstdint>
#include <cstring>
#include "bus_tables.hpp"
#include "region.hpp"
#include "widths.hpp"
#include "timing.hpp"

namespace gba {

// THE interface of this chapter: every bus operation returns payload and
// cost together, so downstream systems (CPU fetch, DMA, PPU) stay timing-
// honest without re-deriving wait states.
struct BusResult {
    uint32_t value;
    unsigned cycles;
};

class Bus {
public:
    static constexpr uint32_t kEwramSize = 256 * 1024;
    static constexpr uint32_t kIwramSize = 32 * 1024;
    static constexpr uint32_t kSmallSize = 1024;   // IO/Palette/OAM
    static constexpr uint32_t kVramSize  = 96 * 1024;
    static constexpr uint32_t kRomSize   = 64 * 1024;   // per chip (fixture)
    static constexpr uint32_t kSramSize  = 32 * 1024;

    uint8_t ewram[kEwramSize] = {};
    uint8_t iwram[kIwramSize] = {};
    uint8_t io[kSmallSize] = {};
    uint8_t palette[kSmallSize] = {};
    uint8_t vram[kVramSize] = {};
    uint8_t oam[kSmallSize] = {};
    uint8_t rom0[kRomSize] = {};
    uint8_t rom1[kRomSize] = {};
    uint8_t rom2[kRomSize] = {};
    uint8_t sram[kSramSize] = {};

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Route, mirror, read at the access width (with ARM load rotation for
    // unaligned words), and account cycles. Unmapped space answers from
    // the open-bus latch.
    BusResult read(uint32_t addr, unsigned width) {
        const Region r = route(addr);
        if (r == Region::OpenBus) return {last_bus_, 1};
        uint32_t v = 0;
        switch (r) {
        case Region::Bios:
        case Region::Ewram:
            v = load(ewram, ewram_canonical(addr), width);
            break;
        case Region::Iwram:
            v = load(iwram, iwram_canonical(addr), width);
            break;
        case Region::Io:       v = load(io, addr & 0x3FFu, width); break;
        case Region::Palette:  v = load(palette, addr & 0x3FFu, width); break;
        case Region::Oam:      v = load(oam, addr & 0x3FFu, width); break;
        case Region::Vram:
            v = load(vram, vram_canonical(addr), width);
            break;
        case Region::RomWs0:
            v = load(rom0, rom_offset<0>(addr), width);
            break;
        case Region::RomWs1:
            v = load(rom1, rom_offset<1>(addr), width);
            break;
        case Region::RomWs2:
            v = load(rom2, rom_offset<2>(addr), width);
            break;
        case Region::Sram:
            v = sram[(addr & 0x7FFFu) % kSramSize];
            break;
        default: return {last_bus_, 1};
        }
        last_bus_ = v;
        return {v, access_cycles(r, seq_)};
    }
//@LABS-STUB
    BusResult read(uint32_t addr, unsigned width) {
        // TODO(1): route the address; answer OpenBus from the latch;
        // otherwise read the region array through the width unit and
        // return BusResult{value, access_cycles(region, seq_)}.
        (void)addr; (void)width;
        return {0, 1};
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // Store at the exact byte addresses (no rotation on writes) and pay
    // the same wait-state cost as a read.
    void write(uint32_t addr, unsigned width, uint32_t value) {
        const Region r = route(addr);
        switch (r) {
        case Region::Ewram: store(ewram, ewram_canonical(addr), width, value); break;
        case Region::Iwram: store(iwram, iwram_canonical(addr), width, value); break;
        case Region::Io:    store(io, addr & 0x3FFu, width, value); break;
        case Region::Palette: store(palette, addr & 0x3FFu, width, value); break;
        case Region::Oam:   store(oam, addr & 0x3FFu, width, value); break;
        case Region::Vram:  store(vram, vram_canonical(addr), width, value); break;
        case Region::RomWs0: store(rom0, rom_offset<0>(addr), width, value); break;
        case Region::RomWs1: store(rom1, rom_offset<1>(addr), width, value); break;
        case Region::RomWs2: store(rom2, rom_offset<2>(addr), width, value); break;
        case Region::Sram:
            sram[addr & 0x7FFFu] = static_cast<uint8_t>(value);
            break;
        default: break;                       // writes to unmapped: dropped
        }
    }
//@LABS-STUB
    void write(uint32_t addr, unsigned width, uint32_t value) {
        // TODO(2): route and store `width` bytes into the right region
        // array (SRAM is byte-wide). OpenBus writes are dropped.
        (void)addr; (void)width; (void)value;
    }
//@LABS-END

    // Pipeline-refill notification: the next access is non-sequential.
    void notify_refill() { seq_ = false; }
    // Track adjacency so the next read can be sequential.
    void note_access(uint32_t addr, unsigned width) {
        seq_ = is_sequential(prev_addr_, addr, width, false);
        prev_addr_ = addr;
    }

private:
    template <int Chip>
    static uint32_t rom_offset(uint32_t addr) {
        return addr & 0xFFFFu;   // fixture chips are 64 K windows
    }

    static uint32_t load(const uint8_t* mem, uint32_t off, unsigned width) {
        uint32_t v = 0;
        const uint32_t base = off & ~(width - 1u);
        for (unsigned i = 0; i < width; ++i)
            v |= static_cast<uint32_t>(mem[base + i]) << (i * 8);
        return width == 1 ? v : rotate_load(off, width, v);
    }
    static void store(uint8_t* mem, uint32_t off, unsigned width,
                      uint32_t value) {
        for (unsigned i = 0; i < width; ++i)
            mem[off + i] = static_cast<uint8_t>(value >> (i * 8));
    }

    uint32_t last_bus_ = 0;
    bool seq_ = false;
    uint32_t prev_addr_ = 0;
};

}  // namespace gba
