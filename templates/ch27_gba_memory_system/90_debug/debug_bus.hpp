#pragma once
#include <cstdint>
#include "bus_tables.hpp"
#include "region.hpp"
#include "widths.hpp"
#include "timing.hpp"

struct BusResult {
    uint32_t value;
    unsigned cycles;
};
using gba::Region;
using gba::route;
using gba::access_cycles;
using gba::is_sequential;
using gba::rotate_load;

namespace debugbus {

// Debuggable GBA bus: same model as 04_bus_result but FIVE defects are
// seeded in the skeleton. Symptoms are catalogued in DEBUGGING.md; the
// hidden grader hashes cycle totals and read values against the fixed
// reference.
struct DebugBus {
    static constexpr uint32_t kEwramSize = 256 * 1024;
    static constexpr uint32_t kIwramSize = 32 * 1024;
    static constexpr uint32_t kVramSize  = 96 * 1024;

    uint8_t ewram[kEwramSize] = {};
    uint8_t iwram[kIwramSize] = {};
    uint8_t vram[kVramSize] = {};
    uint8_t sram[32 * 1024] = {};

    uint32_t last_bus = 0;
    bool seq = false;
    bool refill_pending = false;
    uint32_t prev_addr = 0;

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // A pipeline refill forces the NEXT access to be non-sequential even
    // if its address happens to be adjacent to the previous one.
    void notify_refill() { refill_pending = true; }
//@LABS-STUB
    // BUG(1): refills are swallowed — post-branch accesses bill at the
    // sequential price whenever addresses look adjacent.
    void notify_refill() { (void)seq; }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // EWRAM mirrors every 256 K inside 0x02FFFFFF.
    uint32_t canonical_ewram(uint32_t addr) const { return addr & 0x3FFFFu; }
//@LABS-STUB
    // BUG(2): only the first 64 K of EWRAM is reachable; higher offsets
    // alias back into it.
    uint32_t canonical_ewram(uint32_t addr) const { return addr & 0xFFFFu; }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // VRAM routing per LECTURE.md: 128 K window fold, top hole reflects
    // BG upper half, OBJ bank direct (no naive & 0xFFFF).
    uint32_t canonical_vram(uint32_t addr) const {
        uint32_t off = addr & 0x1FFFFu;
        if (off >= 0x18000u) off -= 0x10000u;
        return off;
    }
//@LABS-STUB
    // BUG(3): a plain 16-bit fold aliases the OBJ bank onto BG memory —
    // sprite tiles appear over background data.
    uint32_t canonical_vram(uint32_t addr) const { return addr & 0xFFFFu; }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // SRAM sits on an 8-bit bus: reads answer exactly one byte.
    BusResult sram_read(uint32_t addr) {
        return {sram[addr & 0x7FFFu], access_cycles(Region::Sram, seq)};
    }
//@LABS-STUB
    // BUG(4): SRAM answers like a 16-bit chip — phantom bytes leak from
    // neighbouring cells.
    BusResult sram_read(uint32_t addr) {
        const uint16_t v = static_cast<uint16_t>(
            sram[addr & 0x7FFFu] | (sram[(addr + 1) & 0x7FFFu] << 8));
        return {v, access_cycles(Region::Sram, seq)};
    }
//@LABS-END

    //@LABS-BEGIN 5
//@LABS-SOLUTION
    // Unmapped reads answer from the last value driven on the data bus.
    BusResult open_bus_read() { return {last_bus, 1}; }
//@LABS-STUB
    // BUG(5): unmapped reads return zero instead of the latched bus value,
    // so code reading write-only registers sees zeros.
    BusResult open_bus_read() { return {0, 1}; }
//@LABS-END

    void note_access(uint32_t addr, unsigned width) {
        seq = !refill_pending &&
              is_sequential(prev_addr, addr, width, false);
        refill_pending = false;
        prev_addr = addr;
    }

    BusResult read(uint32_t addr, unsigned width) {
        const Region r = route(addr);
        if (r == Region::OpenBus) {
            const BusResult ob = open_bus_read();
            last_bus = ob.value;
            return ob;
        }
        uint32_t v = 0;
        unsigned cyc = 1;
        switch (r) {
        case Region::Ewram: {
            uint32_t a = canonical_ewram(addr);
            for (unsigned i = 0; i < width; ++i)
                v |= static_cast<uint32_t>(ewram[a + i]) << (i * 8);
            if (width > 1) v = rotate_load(a, width, v);
            cyc = access_cycles(r, seq);
            break;
        }
        case Region::Iwram: {
            uint32_t a = addr & 0x7FFFu;
            for (unsigned i = 0; i < width; ++i)
                v |= static_cast<uint32_t>(iwram[a + i]) << (i * 8);
            break;
        }
        case Region::Vram: {
            uint32_t a = canonical_vram(addr);
            for (unsigned i = 0; i < width; ++i)
                v |= static_cast<uint32_t>(vram[a + i]) << (i * 8);
            if (width > 1) v = rotate_load(a, width, v);
            break;
        }
        case Region::Sram: return sram_read(addr);
        default: break;
        }
        last_bus = v;
        return {v, cyc};
    }

    void write(uint32_t addr, unsigned width, uint32_t value) {
        const Region r = route(addr);
        switch (r) {
        case Region::Ewram: {
            const uint32_t a = canonical_ewram(addr);
            for (unsigned i = 0; i < width; ++i)
                ewram[a + i] = static_cast<uint8_t>(value >> (i * 8));
            break;
        }
        case Region::Iwram: {
            const uint32_t a = addr & 0x7FFFu;
            for (unsigned i = 0; i < width; ++i)
                iwram[a + i] = static_cast<uint8_t>(value >> (i * 8));
            break;
        }
        case Region::Vram: {
            const uint32_t a = canonical_vram(addr);
            for (unsigned i = 0; i < width; ++i)
                vram[a + i] = static_cast<uint8_t>(value >> (i * 8));
            break;
        }
        case Region::Sram:
            sram[addr & 0x7FFFu] = static_cast<uint8_t>(value);
            break;
        default: break;
        }
        last_bus = value;
    }
};

}  // namespace debugbus
