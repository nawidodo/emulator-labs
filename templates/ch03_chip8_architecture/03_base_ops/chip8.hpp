#pragma once
// chip8.hpp — CHIP-8 core with fetch, field extraction and the five base
// instructions of TODO4: 00E0 CLS, 1NNN JP, 6XNN LD Vx, 7XNN ADD Vx, ANNN LD I.
//
// State/reset/load and the fetch stage were built in exercises 01-02.
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

// Pure field extraction (exercise 02), repeated here so each exercise stays
// self-contained. X is bits 11..8 of the opcode.
inline constexpr uint16_t fetch_word(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) << 8 | p[1]);
}
inline constexpr uint16_t nnn(uint16_t op) { return op & 0x0FFF; }
inline constexpr uint8_t nn(uint16_t op) { return op & 0xFF; }
inline constexpr uint8_t reg_x(uint16_t op) { return (op >> 8) & 0xF; }

class Chip8 {
public:
    void reset();
    void load(std::span<const uint8_t> rom);
    // Fetch, advance PC past the instruction, execute (§56).
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
    bool pixel(size_t x, size_t y) const {
        return display_[y * kScreenWidth + x] != 0;
    }

    void poke_mem(uint16_t addr, uint8_t value) { memory_[addr] = value; }
    std::array<uint8_t, kScreenWidth * kScreenHeight>& pixels() {
        return display_;
    }

private:
    void execute(uint16_t op);
    void op_cls();                  // 00E0
    void op_jp(uint16_t op);        // 1NNN
    void op_ld_vx_nn(uint16_t op);  // 6XNN
    void op_add_vx_nn(uint16_t op); // 7XNN
    void op_ld_i(uint16_t op);      // ANNN

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
            break;  // remaining 0xxx arrive in chapter 4 (00EE RET)
        case 0x1: op_jp(op); break;
        case 0x6: op_ld_vx_nn(op); break;
        case 0x7: op_add_vx_nn(op); break;
        case 0xA: op_ld_i(op); break;
        default: break;  // unknown opcodes are NOP until chapter 4
    }
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// 00E0 CLS: every pixel off.
inline void Chip8::op_cls() { display_.fill(0); }
//@LABS-STUB
inline void Chip8::op_cls() {
    // TODO(1): clear all 64*32 pixels.
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// 1NNN JP: jump to address NNN.
inline void Chip8::op_jp(uint16_t op) { pc_ = nnn(op); }
//@LABS-STUB
inline void Chip8::op_jp(uint16_t op) {
    // TODO(2): set PC to the NNN field.
    (void)op;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// 6XNN LD Vx, NN: load immediate into register VX.
inline void Chip8::op_ld_vx_nn(uint16_t op) { v_[reg_x(op)] = nn(op); }
//@LABS-STUB
inline void Chip8::op_ld_vx_nn(uint16_t op) {
    // TODO(3): set register X to NN. X is bits 11..8 of the opcode.
    (void)op;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// 7XNN ADD Vx, NN: 8-bit wraparound addition, no flags touched.
inline void Chip8::op_add_vx_nn(uint16_t op) {
    v_[reg_x(op)] = static_cast<uint8_t>(v_[reg_x(op)] + nn(op));
}
//@LABS-STUB
inline void Chip8::op_add_vx_nn(uint16_t op) {
    // TODO(4): add NN to register X, wrapping modulo 256.
    (void)op;
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// ANNN LD I, NNN.
inline void Chip8::op_ld_i(uint16_t op) { i_ = nnn(op); }
//@LABS-STUB
inline void Chip8::op_ld_i(uint16_t op) {
    // TODO(5): set I to the NNN field.
    (void)op;
}
//@LABS-END

}  // namespace chip8
