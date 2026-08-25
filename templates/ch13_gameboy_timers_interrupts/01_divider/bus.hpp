#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace gb {

// Self-contained copy of the chapter-11 CPU interface, trimmed to what
// timer programs need. Original authored in ch11 (01_daa_rotates/bus.hpp);
// chapter 13 carries its own copy so every exercise directory tree stands
// alone. Full bus routing is chapter 12's subject.

struct Bus {
    virtual ~Bus() = default;
    [[nodiscard]] virtual uint8_t read(uint16_t address) = 0;
    virtual void write(uint16_t address, uint8_t value) = 0;
};

// Minimal test double for the CPU phase: a flat 64 KiB address space.
struct FlatBus : Bus {
    std::array<uint8_t, 0x10000> mem{};

    [[nodiscard]] uint8_t read(uint16_t address) override { return mem[address]; }
    void write(uint16_t address, uint8_t value) override { mem[address] = value; }

    // Fixture images are assembled at offset 0 and loaded at `base`. The
    // SM83 reset vector fetches from $0100, and vector-page images (ISR at
    // $0050) are loaded with base $0000 instead.
    void load(std::span<const uint8_t> image, uint16_t base = 0x0100) {
        uint16_t addr = base;
        for (const uint8_t b : image) mem[addr++] = b;
    }
};

}  // namespace gb
