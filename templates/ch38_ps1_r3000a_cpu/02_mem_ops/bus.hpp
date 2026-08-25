#pragma once
#include <cstdint>
#include <cstring>

namespace psx::r3000a {

// Minimal RAM-only bus for the CPU labs. The PS1 physical map is expanded in
// ch39; here everything below 2 MB is RAM, and the kuseg/kseg0/kseg1 mirrors
// (0x00000000 / 0x80000000 / 0xA0000000 prefixes) all land on the same memory
// because we strip the top three bits exactly like the real segment decoder.
constexpr uint32_t kRamSize = 2u * 1024u * 1024u;

class Bus {
public:
    uint8_t ram[kRamSize] = {};

    uint8_t* resolve(uint32_t addr) {
        addr &= 0x1FFFFFFFu;  // strip virtual-segment prefix bits
        return addr < kRamSize ? &ram[addr] : nullptr;
    }
    const uint8_t* resolve(uint32_t addr) const {
        addr &= 0x1FFFFFFFu;
        return addr < kRamSize ? &ram[addr] : nullptr;
    }

    // Little-endian scalar accesses, matching the R3000A data layout.
    // Callers keep addresses aligned: on hardware a misaligned lw/lh raises
    // an AdEL/AdES exception (ch39), and the unaligned pair LWL/LWR exists
    // precisely to avoid it.
    uint8_t read8(uint32_t addr) const {
        const uint8_t* p = resolve(addr);
        return p ? *p : uint8_t{0};
    }
    uint16_t read16(uint32_t addr) const {
        const uint8_t* p = resolve(addr);
        if (!p) return 0;
        return uint16_t(p[0]) | uint16_t(p[1]) << 8;
    }
    uint32_t read32(uint32_t addr) const {
        const uint8_t* p = resolve(addr);
        if (!p) return 0;
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
               uint32_t(p[3]) << 24;
    }
    void write8(uint32_t addr, uint8_t v) {
        uint8_t* p = resolve(addr);
        if (p) *p = v;
    }
    void write16(uint32_t addr, uint16_t v) {
        uint8_t* p = resolve(addr);
        if (!p) return;
        p[0] = uint8_t(v);
        p[1] = uint8_t(v >> 8);
    }
    void write32(uint32_t addr, uint32_t v) {
        uint8_t* p = resolve(addr);
        if (!p) return;
        p[0] = uint8_t(v);
        p[1] = uint8_t(v >> 8);
        p[2] = uint8_t(v >> 16);
        p[3] = uint8_t(v >> 24);
    }

    void store_bytes(uint32_t addr, const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) write8(addr + uint32_t(i), data[i]);
    }
};

}  // namespace psx::r3000a
