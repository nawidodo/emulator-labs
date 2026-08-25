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

    // Divergence switches; no opcode in THIS exercise consumes them yet -
    // declared here so the machine layout is stable from day one.
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
    // 1NNN: unconditional jump. BNNN: jump to NNN + V0 (the oddball AXV0
    // form some VIP programs rely on); result wraps at 12 bits.
    bool op_jump(uint16_t op) {
        const uint16_t nnn = op & 0x0FFFu;
        const uint16_t target =
            (op >> 12) == 0xB ? uint16_t(nnn + v[0]) : nnn;
        pc = wrap12(target);
        return true;
    }
//@LABS-STUB
    // TODO({n}): implement 1NNN jump and BNNN jump-to-NNN-plus-V0.
    bool op_jump(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END
//@LABS-BEGIN 2
//@LABS-SOLUTION
    // 2NNN call / 00EE return. fetch() already advanced PC past the call,
    // so pushing PC pushes the natural return address. Overflow and underflow
    // halt rather than silently corrupting memory.
    bool op_call(uint16_t op) {
        if (op == 0x00EE) {
            if (sp == 0) return false;  // RET with empty stack: fault
            --sp;
            pc = stack[sp];
            return true;
        }
        if (sp >= kStackSlots) return false;  // nested too deep: fault
        stack[sp++] = pc;
        pc = wrap12(uint16_t(op & 0x0FFFu));
        return true;
    }
//@LABS-STUB
    // TODO({n}): implement 2NNN call (push return address) and 00EE return.
    bool op_call(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END
//@LABS-BEGIN 3
//@LABS-SOLUTION
    // 3XNN skip-if-equal, 4XNN skip-if-not-equal. Skipping means stepping
    // over the 2-byte NEXT instruction on top of fetch()'s own advance.
    bool op_skip_imm(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const bool equal = v[x] == (op & 0xFFu);
        const bool se_form = (op >> 12) == 0x3;
        if (equal == se_form) pc = wrap12(uint16_t(pc + 2));
        return true;
    }
//@LABS-STUB
    // TODO({n}): implement 3XNN (skip if VX==NN) and 4XNN (skip if VX!=NN).
    bool op_skip_imm(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END
//@LABS-BEGIN 4
//@LABS-SOLUTION
    // 5XY0 skip-if-equal, 9XY0 skip-if-not-equal (register forms).
    bool op_skip_reg(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const bool equal = v[x] == v[y];
        const bool se_form = (op >> 12) == 0x5;
        if (equal == se_form) pc = wrap12(uint16_t(pc + 2));
        return true;
    }
//@LABS-STUB
    // TODO({n}): implement 5XY0 (skip if VX==VY) and 9XY0 (skip if VX!=VY).
    bool op_skip_reg(uint16_t op) {
        (void)op;
        return false;  // stub halts the machine; suite runs RED until done
    }
//@LABS-END

    // Opcode dispatcher: family nibble -> handler. Returning false marks an
    // unsupported/illegal opcode and halts the machine via step().
    bool exec(uint16_t op) {
        switch (op >> 12) {
            case 0x0:
                if (op == 0x00EE) return op_call(op);
                return false;   // other 0nnn codes (display clear) are ch05
            case 0x1: case 0xB: return op_jump(op);
            case 0x2:           return op_call(op);
            case 0x3: case 0x4: return op_skip_imm(op);
            case 0x5: case 0x9: return op_skip_reg(op);
            default:  return false;  // ALU/memory arrive in TODO2..TODO5
        }
    }
};

}  // namespace chip8
