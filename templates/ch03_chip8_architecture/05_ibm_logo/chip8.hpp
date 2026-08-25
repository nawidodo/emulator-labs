#pragma once
// chip8.hpp — full chapter core plus the minimal sprite blit the challenge
// needs: DXYN draws an 8-wide XOR sprite from memory[I] at (VX, VY).
//
// Everything except op_drw was built across exercises 01-04 and ships done;
// the challenge is making tests/public/ch03_chip8_architecture/roms/
// ibm_logo.ch8 render correctly under the headless runner.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <cstdio>

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

// Pure field extraction (exercise 02).
inline constexpr uint16_t fetch_word(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) << 8 | p[1]);
}
inline constexpr uint16_t nnn(uint16_t op) { return op & 0x0FFF; }
inline constexpr uint8_t nn(uint16_t op) { return op & 0xFF; }
inline constexpr uint8_t imm4(uint16_t op) { return op & 0xF; }
inline constexpr uint8_t reg_x(uint16_t op) { return (op >> 8) & 0xF; }
inline constexpr uint8_t reg_y(uint16_t op) { return (op >> 4) & 0xF; }

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

    // §55: every CPU has a disassembler from day one.
    std::string disassemble(uint16_t pc) const;

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
    const std::array<uint8_t, kScreenWidth * kScreenHeight>& pixels() const {
        return display_;
    }

private:
    void execute(uint16_t op);
    void op_cls();                   // 00E0
    void op_jp(uint16_t op);         // 1NNN
    void op_ld_vx_nn(uint16_t op);   // 6XNN
    void op_add_vx_nn(uint16_t op);  // 7XNN
    void op_ld_i(uint16_t op);       // ANNN
    void op_drw(uint16_t op);        // DXYN (minimal blit for this chapter)

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
        case 0xD: op_drw(op); break;
        default: break;  // unknown opcodes are NOP until chapter 4
    }
}

inline void Chip8::op_cls() { display_.fill(0); }
inline void Chip8::op_jp(uint16_t op) { pc_ = nnn(op); }
inline void Chip8::op_ld_vx_nn(uint16_t op) { v_[reg_x(op)] = nn(op); }
inline void Chip8::op_add_vx_nn(uint16_t op) {
    v_[reg_x(op)] = static_cast<uint8_t>(v_[reg_x(op)] + nn(op));
}
inline void Chip8::op_ld_i(uint16_t op) { i_ = nnn(op); }

//@LABS-BEGIN 1
//@LABS-SOLUTION
// DXYN: XOR-blit an 8-pixel-wide, N-row sprite from memory[I] onto the
// framebuffer at (VX, VY). VF is the collision flag: 1 iff any LIT pixel was
// erased (XOR with a set pixel). Rows/columns past the screen edge are
// clipped — wrapping is Chapter 5 material.
inline void Chip8::op_drw(uint16_t op) {
    const size_t ox = v_[reg_x(op)];
    const size_t oy = v_[reg_y(op)];
    const uint8_t rows = imm4(op);
    v_[0xF] = 0;
    for (uint8_t r = 0; r < rows; ++r) {
        const uint8_t bits = memory_[i_ + r];
        for (uint8_t b = 0; b < 8; ++b) {
            if (!(bits & (0x80u >> b))) continue;
            const size_t sx = ox + b;
            const size_t sy = oy + r;
            if (sx >= kScreenWidth || sy >= kScreenHeight) continue;
            auto& px = display_[sy * kScreenWidth + sx];
            if (px) v_[0xF] = 1;  // we are about to erase a lit pixel
            px ^= 1;
        }
    }
}
//@LABS-STUB
inline void Chip8::op_drw(uint16_t op) {
    // TODO(1): XOR-blit the sprite at memory[I] (imm4(op) rows, 8 columns)
    // at (Vx, Vy); set VF if any lit pixel gets erased. Clip at screen edges.
    (void)op;
}
//@LABS-END

// §55 disassembler, extended with the DRW line this chapter introduces.
inline std::string Chip8::disassemble(uint16_t pc) const {
    const uint16_t op = fetch_word(&memory_[pc]);
    char line[48];
    switch (op >> 12) {
        case 0x0:
            if (op == 0x00E0)
                std::snprintf(line, sizeof line, "%04X: %04X  CLS", pc, op);
            else
                std::snprintf(line, sizeof line, "%04X: %04X  DW 0x%04X", pc,
                              op, op);
            break;
        case 0x1:
            std::snprintf(line, sizeof line, "%04X: %04X  JP 0x%03X", pc, op,
                          nnn(op));
            break;
        case 0x6:
            std::snprintf(line, sizeof line, "%04X: %04X  LD V%X, 0x%02X", pc,
                          op, reg_x(op), nn(op));
            break;
        case 0x7:
            std::snprintf(line, sizeof line, "%04X: %04X  ADD V%X, 0x%02X", pc,
                          op, reg_x(op), nn(op));
            break;
        case 0xA:
            std::snprintf(line, sizeof line, "%04X: %04X  LD I, 0x%03X", pc,
                          op, nnn(op));
            break;
        case 0xD:
            std::snprintf(line, sizeof line, "%04X: %04X  DRW V%X, V%X, %u",
                          pc, op, reg_x(op), reg_y(op), imm4(op));
            break;
        default:
            std::snprintf(line, sizeof line, "%04X: %04X  DW 0x%04X", pc, op,
                          op);
            break;
    }
    return line;
}

}  // namespace chip8
