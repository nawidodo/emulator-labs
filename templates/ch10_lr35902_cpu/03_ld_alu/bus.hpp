#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace gb {

struct Bus {
    virtual ~Bus() = default;
    [[nodiscard]] virtual uint8_t read(uint16_t address) = 0;
    virtual void write(uint16_t address, uint8_t value) = 0;
};

// Minimal test double for the CPU phase: a flat 64 KiB address space.
// Real bus routing is Chapter 12's subject.
struct FlatBus : Bus {
    std::array<uint8_t, 0x10000> mem{};
 
    [[nodiscard]] uint8_t read(uint16_t address) override { return mem[address]; }
    void write(uint16_t address, uint8_t value) override { mem[address] = value; }

    // Fixture images are assembled at ORG $0100 and loaded there; the CPU
    // reset vector points at $0100 as well.
    void load(std::span<const uint8_t> image, uint16_t base = 0x0100) {
        uint16_t addr = base;
        for (const uint8_t b : image) mem[addr++] = b;
    }
};

}  // namespace gb
