#pragma once
#include <cstdint>
#include "bus_tables.hpp"

namespace gba {

// Width-aware access unit over a flat backing store. The GBA data buses
// are 8/16/32 bits wide depending on region; the CPU may issue any width.
// Reads of non-aligned words do NOT fault: hardware fetches the aligned
// word and ROTATES it right by (addr & 3) * 8 so the addressed byte sits
// in bits 0-7 (ARM LDR rotation rule). Writes simply store the value's
// low bytes at the exact byte addresses.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Rotate `value` right by (addr % width_bytes) * 8 — the ARM unaligned
// load rule. Aligned values pass through unchanged.
inline uint32_t rotate_load(uint32_t addr, unsigned width_bytes,
                            uint32_t value) {
    const unsigned shift = (addr & (width_bytes - 1)) * 8;
    if (shift == 0) return value;
    return (value >> shift) | (value << (32 - shift));
}
//@LABS-STUB
inline uint32_t rotate_load(uint32_t addr, unsigned width_bytes,
                            uint32_t value) {
    // TODO(1): rotate the loaded aligned value RIGHT by
    // (addr & (width_bytes - 1)) * 8. Aligned addresses pass through.
    (void)addr; (void)width_bytes; (void)value;
    return value;
}
//@LABS-END

struct WidthBus {
    static constexpr uint32_t kSize = 0x8000;
    uint8_t mem[kSize] = {};

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // Read `width` bytes honoring alignment: assemble the aligned word,
    // then apply the load rotation. Byte reads never rotate.
    uint32_t read(uint32_t addr, unsigned width) {
        const uint32_t base = addr & ~(width - 1u);
        uint32_t v = 0;
        for (unsigned i = 0; i < width; ++i)
            v |= static_cast<uint32_t>(mem[(base + i) & (kSize - 1)]) << (i * 8);
        return width == 1 ? v : rotate_load(addr, width, v);
    }
//@LABS-STUB
    uint32_t read(uint32_t addr, unsigned width) {
        // TODO(2): build the aligned width-byte little-endian value from
        // mem, then rotate per TODO(1). Bytes read directly.
        (void)addr; (void)width;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // Write `width` bytes: store value's low bytes at the exact address
    // bytes (no rotation on stores).
    void write(uint32_t addr, unsigned width, uint32_t value) {
        for (unsigned i = 0; i < width; ++i)
            mem[(addr + i) & (kSize - 1)] =
                static_cast<uint8_t>(value >> (i * 8));
    }
//@LABS-STUB
    void write(uint32_t addr, unsigned width, uint32_t value) {
        // TODO(3): store the low `width` bytes of value starting at addr.
        (void)addr; (void)width; (void)value;
    }
//@LABS-END
};

}  // namespace gba
