#pragma once
// chip8.hpp — chapter core as of this point. One instruction decodes its
// register field incorrectly; see DEBUGGING.md for the reported symptom.
//
// Your job: reproduce with a trace, find the first observable divergence,
// fix the root cause, write bug-report.md (bug / root cause / first
// divergence / fix / regression test).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace chip8 {

inline constexpr size_t kMemSize = 4096;
inline constexpr uint16_t kProgStart = 0x200;
inline constexpr uint16_t kFontAddr = 0x050;
inline constexpr size_t kScreenWidth = 64;
inline constexpr size_t kScreenHeight = 32;
inline constexpr size_t kStackSize = 16;

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
    uint64_t cycles;
    uint16_t pc;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Register selector X: bits 11..8 — the low nibble of the opcode's high byte.
inline constexpr uint8_t reg_x(uint16_t op) { return static_cast<uint8_t>((op >> 8) & 0xF); }
//@LABS-STUB
// Decoder field helpers are being refactored; behavior reviewed in ch03.
inline constexpr uint8_t reg_x(uint16_t op) {
    // TODO(1): confirm nibble selection before the refactor lands.
    return static_cast<uint8_t>(op & 0xF);
}
//@LABS-END

inline constexpr uint16_t fetch_word(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) << 8 | p[1]);
}
inline constexpr uint16_t nnn(uint16_t op) { return op & 0x0FFF; }
inline constexpr uint8_t nn(uint16_t op) { return op & 0xFF; }

class Chip8 {
public:
    void reset();
    void load(std::span<const uint8_t> rom);

    StepResult step() {
        const uint16_t op = fetch_word(&memory_[pc_]);
        last_op_ = op;
        pc_ = static_cast<uint16_t>(pc_ + 2);
        execute(op);
        ++cycles_;
        return {cycles_, pc_};
    }

    uint16_t last_op() const { return last_op_; }
    uint8_t mem(uint16_t addr) const { return memory_[addr]; }
    uint8_t v(size_t reg) const { return v_[reg]; }
    uint16_t i() const { return i_; }
    uint16_t pc() const { return pc_; }
    uint8_t sp() const { return sp_; }
    uint8_t delay() const { return delay_timer_; }
    uint8_t sound() const { return sound_timer_; }

private:
    void execute(uint16_t op);
    void op_cls() { display_.fill(0); }
    void op_jp(uint16_t op) { pc_ = nnn(op); }
    void op_ld_vx_nn(uint16_t op) { v_[reg_x(op)] = nn(op); }
    void op_add_vx_nn(uint16_t op) {
        v_[reg_x(op)] = static_cast<uint8_t>(v_[reg_x(op)] + nn(op));
    }
    void op_ld_i(uint16_t op) { i_ = nnn(op); }

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
    uint16_t last_op_ = 0;
};

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
    for (size_t n = 0; n < kFontset.size(); ++n)
        memory_[kFontAddr + n] = kFontset[n];
}

inline void Chip8::load(std::span<const uint8_t> rom) {
    for (size_t n = 0; n < rom.size(); ++n)
        memory_[kProgStart + n] = rom[n];
}

inline void Chip8::execute(uint16_t op) {
    switch (op >> 12) {
        case 0x0:
            if (op == 0x00E0) op_cls();
            break;
        case 0x1: op_jp(op); break;
        case 0x6: op_ld_vx_nn(op); break;
        case 0x7: op_add_vx_nn(op); break;
        case 0xA: op_ld_i(op); break;
        default: break;
    }
}

}  // namespace chip8
