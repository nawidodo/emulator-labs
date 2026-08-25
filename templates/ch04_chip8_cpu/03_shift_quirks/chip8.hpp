#pragma once
// ch04_chip8_cpu - complete CHIP-8 CPU.
// Self-contained copy for this exercise (docs/AUTHORING.md layout rule):
// earlier instruction families arrive as finished code so every exercise
// builds on the same coherent machine. @LABS blocks mark THIS exercise's tasks.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

namespace chip8 {

// Quirk switches: one bit per historically divergent behavior. The reference
// implementation routes EVERY divergence through this struct instead of
// scattered special cases (curriculum ch04 solution requirement).
// Default profile = COSMAC VIP ("classic"); "modern" = CHIP-48/HIP-8 style,
// see LECTURE.md and toggle via the runners' --quirks flag.
struct Chip8Quirks {
    bool shift_uses_vy = false;       // true: 8XY6/E shifts VY into VX (CHIP-48)
    bool load_store_leaves_i = false; // true: FX55/FX65 leave I unchanged (CHIP-48)
    bool vf_reset = false;            // true: FX55/FX65 clear VF (some CHIP-48 builds)
    bool wrapping = true;             // true: I/PC arithmetic wraps at 0x1000 (VIP)
};

class Chip8 {
public:
    static constexpr uint16_t kMemSize = 4096;
    static constexpr uint16_t kProgramBase = 0x0200;
    static constexpr int kStackSlots = 16;

    uint8_t  v[16] = {};           // data registers V0-VF; VF doubles as flag
    uint16_t idx = 0;              // I register ('idx': bare 'i' reads badly)
    uint16_t pc = kProgramBase;
    uint8_t  sp = 0;               // counts occupied stack slots
    uint16_t stack[kStackSlots] = {};
    uint8_t  dtimer = 0;           // delay timer (ticked at 60 Hz from ch05 on)
    uint8_t  stimer = 0;           // sound timer
    uint8_t  mem[kMemSize] = {};   // 4 KiB RAM: font at 0x000, ROM at 0x200
    bool     key[16] = {};         // keypad state, driven by host/tests
    uint64_t cycles = 0;
    bool     halted = false;       // set on illegal opcode or stack fault
    uint16_t illegal_op = 0;

    // Divergence switches (see Chip8Quirks above); default = COSMAC.
    Chip8Quirks quirks;

    // Reset to power-on state; hex digit font maps at 0x000-0x04F.
    void reset() {
        *this = Chip8{};
        static const uint8_t kFont[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // '0'
        0x20, 0x60, 0x20, 0x20, 0x70,  // '1'
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // '2'
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // '3'
        0x90, 0x90, 0xF0, 0x10, 0x10,  // '4'
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // '5'
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // '6'
        0xF0, 0x10, 0x20, 0x40, 0x40,  // '7'
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // '8'
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // '9'
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // 'A'
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // 'B'
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // 'C'
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // 'D'
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // 'E'
        0xF0, 0x80, 0xF0, 0x80, 0x80,  // 'F'
        };
        std::memcpy(mem, kFont, sizeof(kFont));
    }

    // Copy ROM at 0x0200 (VIP convention); oversized loads truncate.
    void load(std::span<const uint8_t> rom) {
        const size_t n = std::min(rom.size(), size_t{kMemSize - kProgramBase});
        std::memcpy(mem + kProgramBase, rom.data(), n);
    }

    // Every CHIP-8 address lives in a 12-bit space; arithmetic wraps there.
    static uint16_t wrap12(uint16_t a) { return uint16_t(a & 0x0FFFu); }

    // Fetch big-endian opcode at PC and advance past it. The advance happens
    // here so skip/jump handlers below only ever add the extra 2 bytes.
    uint16_t fetch() {
        const uint16_t op =
            uint16_t(uint16_t(mem[pc]) << 8 | mem[wrap12(uint16_t(pc + 1))]);
        pc = wrap12(uint16_t(pc + 2));
        ++cycles;
        return op;
    }

    // Execute exactly one instruction (curriculum SS56 stepping contract).
    // Sets halted on an illegal opcode or stack fault so headless runs end
    // deterministically instead of spinning forever.
    uint16_t step() {
        if (halted) return 0;
        const uint16_t op = fetch();
        if (!exec(op)) {
            halted = true;
            illegal_op = op;
        }
        return op;
    }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // 8XY6 right shift. The quirk selects the SOURCE operand: COSMAC VIP
    // shifts VY into VX, CHIP-48/HIP-8 shift VX in place (Y unused). VF
    // captures bit 0 BEFORE the shift - reading it afterwards yields 0.
    bool op_shift_right(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint8_t src = quirks.shift_uses_vy ? v[y] : v[x];
        v[0xF] = src & 0x1u;
        v[x] = uint8_t(src >> 1);
        return true;
    }
//@LABS-STUB
    // TODO(1): implement 8XY6 honoring quirks.shift_uses_vy;
    // VF = shifted-out bit captured BEFORE the shift.
    bool op_shift_right(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END
//@LABS-BEGIN 2
//@LABS-SOLUTION
    // 8XYE left shift: same source-selection quirk, VF = old bit 7.
    bool op_shift_left(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint8_t src = quirks.shift_uses_vy ? v[y] : v[x];
        v[0xF] = uint8_t(src >> 7);
        v[x] = uint8_t(src << 1);
        return true;
    }
//@LABS-STUB
    // TODO(2): implement 8XYE with the same source-selection rule.
    bool op_shift_left(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END
    // 6XNN load immediate, 7XNN add immediate. 7XNN deliberately wraps
    // WITHOUT touching VF: only the register-form ALU sets flags.
    bool op_imm(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t nn = uint8_t(op & 0xFFu);
        if ((op >> 12) == 0x6) v[x] = nn;
        else v[x] = uint8_t(v[x] + nn);
        return true;
    }

    // 8XY0 move, 8XY1 OR, 8XY2 AND, 8XY3 XOR. None of these touch VF.
    bool op_logic(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        switch (op & 0xFu) {
            case 0x0: v[x] = v[y]; break;
            case 0x1: v[x] |= v[y]; break;
            case 0x2: v[x] &= v[y]; break;
            default:  v[x] ^= v[y]; break;  // 0x3
        }
        return true;
    }

    // 8XY4: VX = VX + VY, VF = carry out of bit 7.
    bool op_add_carry(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint16_t sum = uint16_t(uint16_t(v[x]) + v[y]);
        v[x] = uint8_t(sum & 0xFFu);
        v[0xF] = sum > 0xFF ? 1 : 0;
        return true;
    }

    // 8XY5: VX -= VY, VF = NOT borrow (1 when no underflow happened).
    // 8XY7: VX = VY - VX, same flag convention. Note the asymmetry trap:
    // the SOURCE of the subtraction differs, the FLAG semantics do not -
    // VF is always "the subtraction fit in 8 bits".
    bool op_sub_borrow(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        if ((op & 0xFu) == 0x5) {
            v[0xF] = v[x] >= v[y] ? 1 : 0;
            v[x] = uint8_t(v[x] - v[y]);
        } else {
            v[0xF] = v[y] >= v[x] ? 1 : 0;
            v[x] = uint8_t(v[y] - v[x]);
        }
        return true;
    }

    // Route the 8XY* family by low nibble. Unassigned codes fall through to
    // 'false' -> halted with illegal_op recorded.
    bool op_alu(uint16_t op) {
        switch (op & 0xFu) {
            case 0x0: case 0x1: case 0x2: case 0x3:
                return op_logic(op);
            case 0x4:
                return op_add_carry(op);
            case 0x5: case 0x7:
                return op_sub_borrow(op);
            case 0x6:
                return op_shift_right(op);
            case 0xE:
                return op_shift_left(op);
            default:
                return false;
        }
    }


    // Opcode dispatcher: family nibble -> handler. Returning false marks an
    // unsupported/illegal opcode and halts the machine via step().
    bool exec(uint16_t op) {
        switch (op >> 12) {
            case 0x6: case 0x7: return op_imm(op);
            case 0x8:           return op_alu(op);
            default:  return false;
        }
    }
};

}  // namespace chip8
