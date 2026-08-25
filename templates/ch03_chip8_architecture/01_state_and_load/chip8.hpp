#pragma once
// chip8.hpp — CHIP-8 machine state, reset and ROM loading.
//
// The machine is a plain struct-of-arrays: every device (memory, registers,
// timers, framebuffer) is independently inspectable from tests (§57).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace chip8 {

inline constexpr size_t kMemSize = 4096;
inline constexpr uint16_t kProgStart = 0x200;   // ROMs load here
inline constexpr uint16_t kFontAddr = 0x050;    // interpreter area
inline constexpr size_t kScreenWidth = 64;
inline constexpr size_t kScreenHeight = 32;
inline constexpr size_t kStackSize = 16;

// Canonical 4x5 digit sprites the original interpreter kept at 0x050.
// Chapter 5's FX29 points I at these; committed here so reset() can install
// them exactly like the COSMAC VIP did.
inline constexpr std::array<uint8_t, 80> kFontset = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x10, 0x10, 0x10,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};

struct StepResult {
    uint64_t cycles;   // total executed so far
    uint16_t pc;       // PC after the step
};

class Chip8 {
public:
    void reset();
    void load(std::span<const uint8_t> rom);

    // --- read-only views for headless tests ---
    uint8_t mem(uint16_t addr) const { return memory_[addr]; }
    uint8_t v(size_t reg) const { return v_[reg]; }
    uint16_t i() const { return i_; }
    uint16_t pc() const { return pc_; }
    uint8_t sp() const { return sp_; }
    uint8_t delay() const { return delay_timer_; }
    uint8_t sound() const { return sound_timer_; }
    bool pixel(size_t x, size_t y) const {
        return display_[y * kScreenWidth + x] != 0;
    }

    // --- test hooks (§57: drive devices directly, no window needed) ---
    void poke_mem(uint16_t addr, uint8_t value) { memory_[addr] = value; }
    std::array<uint8_t, kScreenWidth * kScreenHeight>& pixels() {
        return display_;
    }

private:
    std::array<uint8_t, kMemSize> memory_{};
    std::array<uint8_t, 16> v_{};
    std::array<uint16_t, kStackSize> stack_{};
    std::array<uint8_t, kScreenWidth * kScreenHeight> display_{};
    uint16_t i_ = 0;
    uint16_t pc_ = kProgStart;
    uint8_t sp_ = 0;
    uint8_t delay_timer_ = 0;
    uint8_t sound_timer_ = 0;
    uint64_t cycles_ = 0;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Reset returns the machine to power-on state: font installed at 0x050,
// everything else zeroed, PC at the ROM entry point.
inline void Chip8::reset() {
    memory_.fill(0);
    v_.fill(0);
    stack_.fill(0);
    display_.fill(0);
    i_ = 0;
    pc_ = kProgStart;
    sp_ = 0;
    delay_timer_ = 0;
    sound_timer_ = 0;
    cycles_ = 0;
    // Font lives in the "interpreter area" — programs must not rely on it
    // being there, but real hardware always provided it.
    for (size_t n = 0; n < kFontset.size(); ++n)
        memory_[kFontAddr + n] = kFontset[n];
}
//@LABS-STUB
inline void Chip8::reset() {
    // TODO(1): zero all state, write kFontset at kFontAddr (0x050),
    // set PC to kProgStart (0x200), SP/timers/cycles to 0.
    (void)kFontset;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// ROM bytes land at 0x200 because the first 512 bytes belonged to the
// interpreter on the original hardware.
inline void Chip8::load(std::span<const uint8_t> rom) {
    for (size_t n = 0; n < rom.size(); ++n)
        memory_[kProgStart + n] = rom[n];
}
//@LABS-STUB
inline void Chip8::load(std::span<const uint8_t> rom) {
    // TODO(2): copy rom bytes into memory starting at kProgStart (0x200).
    (void)rom;
}
//@LABS-END

}  // namespace chip8
