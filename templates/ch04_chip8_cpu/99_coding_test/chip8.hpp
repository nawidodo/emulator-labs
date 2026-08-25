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

    static constexpr uint64_t kRngSeed = 0x5EEDC0488ULL;

    // Deterministic LCG state: reinitialized at reset(), reseedable through
    // seed_rng() so CXNN results are reproducible (no unseeded RNG allowed).
    uint64_t rng_state = kRngSeed;

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
    // MATH-1: 8XY8 AVG
    //   sum = VX + VY          (16-bit, no wrap)
    //   VX  = sum / 2          (integer average)
    //   VF  = sum & 1          (the lost low bit: 1 when the sum was odd)
    bool op_ext_avg(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint16_t sum = uint16_t(uint16_t(v[x]) + v[y]);
        v[x] = uint8_t(sum >> 1);
        v[0xF] = uint8_t(sum & 0x1u);
        return true;
    }
//@LABS-STUB
    // TODO(1): implement this MATH-X opcode exactly as specified
    // in CODING_TEST.md.
    bool op_ext_avg(uint16_t op) {
        (void)op;
        return false;  // unimplemented until you write it
    }
//@LABS-END
//@LABS-BEGIN 2
//@LABS-SOLUTION
    // MATH-2: 8XY9 MIN - if VY < VX, move VY into VX and set VF=1;
    // otherwise leave both alone and set VF=0. (VF = "replacement happened".)
    bool op_ext_min(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        if (v[y] < v[x]) { v[x] = v[y]; v[0xF] = 1; }
        else v[0xF] = 0;
        return true;
    }
//@LABS-STUB
    // TODO(2): implement this MATH-X opcode exactly as specified
    // in CODING_TEST.md.
    bool op_ext_min(uint16_t op) {
        (void)op;
        return false;  // unimplemented until you write it
    }
//@LABS-END
//@LABS-BEGIN 3
//@LABS-SOLUTION
    // MATH-3: 8XYA MAX - if VY > VX, move VY into VX and set VF=1;
    // otherwise leave both alone and set VF=0.
    bool op_ext_max(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        if (v[y] > v[x]) { v[x] = v[y]; v[0xF] = 1; }
        else v[0xF] = 0;
        return true;
    }
//@LABS-STUB
    // TODO(3): implement this MATH-X opcode exactly as specified
    // in CODING_TEST.md.
    bool op_ext_max(uint16_t op) {
        (void)op;
        return false;  // unimplemented until you write it
    }
//@LABS-END
//@LABS-BEGIN 4
//@LABS-SOLUTION
    // MATH-4: 8XYB MULLO - VX = low byte of VX*VY, VF = high byte of the
    // 16-bit product. FF*FF = FE01 is the boundary case.
    bool op_ext_mullo(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const unsigned prod = unsigned(v[x]) * unsigned(v[y]);
        v[x] = uint8_t(prod & 0xFFu);
        v[0xF] = uint8_t(prod >> 8);
        return true;
    }
//@LABS-STUB
    // TODO(4): implement this MATH-X opcode exactly as specified
    // in CODING_TEST.md.
    bool op_ext_mullo(uint16_t op) {
        (void)op;
        return false;  // unimplemented until you write it
    }
//@LABS-END
//@LABS-BEGIN 5
//@LABS-SOLUTION
    // MATH-5: FXY2 XSUM - XOR-fold memory over [I, I+VY] inclusive into VX.
    // I is left unchanged; VF is cleared. Range includes both endpoints.
    bool op_ext_xsum(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        uint8_t acc = 0;
        for (int n = 0; n <= y; ++n)
            acc ^= mem[wrap12(uint16_t(idx + n))];
        v[x] = acc;
        v[0xF] = 0;
        return true;
    }
//@LABS-STUB
    // TODO(5): implement this MATH-X opcode exactly as specified
    // in CODING_TEST.md.
    bool op_ext_xsum(uint16_t op) {
        (void)op;
        return false;  // unimplemented until you write it
    }
//@LABS-END
    // 1NNN: unconditional jump. BNNN: jump to NNN + V0 (the oddball AXV0
    // form some VIP programs rely on); result wraps at 12 bits.
    bool op_jump(uint16_t op) {
        const uint16_t nnn = op & 0x0FFFu;
        const uint16_t target =
            (op >> 12) == 0xB ? uint16_t(nnn + v[0]) : nnn;
        pc = wrap12(target);
        return true;
    }
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
    // 3XNN skip-if-equal, 4XNN skip-if-not-equal. Skipping means stepping
    // over the 2-byte NEXT instruction on top of fetch()'s own advance.
    bool op_skip_imm(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const bool equal = v[x] == (op & 0xFFu);
        const bool se_form = (op >> 12) == 0x3;
        if (equal == se_form) pc = wrap12(uint16_t(pc + 2));
        return true;
    }
    // 5XY0 skip-if-equal, 9XY0 skip-if-not-equal (register forms).
    bool op_skip_reg(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const bool equal = v[x] == v[y];
        const bool se_form = (op >> 12) == 0x5;
        if (equal == se_form) pc = wrap12(uint16_t(pc + 2));
        return true;
    }
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

    // 8XY6 right shift, 8XYE left shift. The quirk selects the SOURCE
    // operand: COSMAC VIP shifts VY into VX, CHIP-48/HIP-8 shift VX in place
    // (making Y a no-op operand). VF captures the bit shifted out BEFORE the
    // shift - reading it afterwards would always yield 0.
    bool op_shift(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint8_t src = quirks.shift_uses_vy ? v[y] : v[x];
        if ((op & 0xFu) == 0x6) {
            v[0xF] = src & 0x1u;
            v[x] = uint8_t(src >> 1);
        } else {
            v[0xF] = uint8_t(src >> 7);
            v[x] = uint8_t(src << 1);
        }
        return true;
    }

    // ANNN: load I with NNN. Grouped with memory ops because every FX**
    // addressing mode below hangs off I.
    bool op_set_i(uint16_t op) {
        idx = uint16_t(op & 0x0FFFu);
        return true;
    }

    // FX29: point I at the 5-byte glyph for the hex digit in VX.
    bool op_font(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        idx = uint16_t(5 * (v[x] & 0x0Fu));
        return true;
    }

    // FX33: store VX as three BCD digits (hundreds, tens, ones). The digit
    // ORDER matters more than the math: most real-world bugs here are
    // swapped digits, not wrong division.
    bool op_bcd(uint16_t op) {
        const uint8_t val = v[(op >> 8) & 0xFu];
        mem[wrap12(idx)]     = uint8_t(val / 100);
        mem[wrap12(idx + 1)] = uint8_t((val / 10) % 10);
        mem[wrap12(idx + 2)] = uint8_t(val % 10);
        return true;
    }

    // FX55: dump V0..VX to memory at I. Two divergent conventions, both
    // routed through quirks: whether I advances by X+1 afterwards, and
    // whether VF gets cleared. Byte-wise addressing wraps at 4 KiB.
    bool op_store_v(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        for (int n = 0; n <= x; ++n) mem[wrap12(uint16_t(idx + n))] = v[n];
        if (!quirks.load_store_leaves_i) idx = wrap12(uint16_t(idx + x + 1));
        if (quirks.vf_reset) v[0xF] = 0;
        return true;
    }

    // FX65: refill V0..VX from memory at I; same two quirks as FX55.
    bool op_load_v(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        for (int n = 0; n <= x; ++n) v[n] = mem[wrap12(uint16_t(idx + n))];
        if (!quirks.load_store_leaves_i) idx = wrap12(uint16_t(idx + x + 1));
        if (quirks.vf_reset) v[0xF] = 0;
        return true;
    }

    // Fixed-seed injection point: tests and fixtures call seed_rng() with a
    // known value before running CXNN sequences.
    void seed_rng(uint64_t s) { rng_state = s; }

    // One LCG step per request (Numerical Recipes constants). The TOP byte
    // is consumed because low LCG bits have short periods.
    uint8_t next_random() {
        rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return uint8_t(rng_state >> 56);
    }

    // CXNN: VX = random byte AND NN - the mask bounds the range; the RNG
    // itself must be seeded so results are reproducible.
    bool op_rand(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        v[x] = uint8_t(next_random() & (op & 0xFFu));
        return true;
    }

    // EX9E skip-if-key(VX) pressed, EXA1 skip-if-not. Key state is scripted
    // by the host/tests; FX0A (blocking wait) belongs to ch05.
    bool op_key(uint16_t op) {
        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t k = v[x] & 0x0Fu;
        switch (op & 0xFFu) {
            case 0x9E: if (key[k])  pc = wrap12(uint16_t(pc + 2)); return true;
            case 0xA1: if (!key[k]) pc = wrap12(uint16_t(pc + 2)); return true;
            default:   return false;
        }
    }

    bool op_mem(uint16_t op) {
        switch (op & 0xFFu) {
            case 0x29: return op_font(op);
            case 0x33: return op_bcd(op);
            case 0x55: return op_store_v(op);
            case 0x65: return op_load_v(op);
            default:
                return false;  // timers (FX07/15/18) and input wait live in ch05
        }
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
            case 0x6: case 0xE:
                return op_shift(op);
            default:
                return false;
        }
    }

    bool op_alu_ext(uint16_t op) {
        switch (op & 0xFu) {
            case 0x8: return op_ext_avg(op);
            case 0x9: return op_ext_min(op);
            case 0xA: return op_ext_max(op);
            case 0xB: return op_ext_mullo(op);
            default:  return false;  // C/D stay unassigned
        }
    }

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
            case 0x6: case 0x7: return op_imm(op);
            case 0x8:
                return (op & 0xFu) >= 0x8 ? op_alu_ext(op) : op_alu(op);
            case 0xA:           return op_set_i(op);
            case 0xC:           return op_rand(op);
            case 0xE:           return op_key(op);
            case 0xF:
                return (op & 0xFFu) == 0xF2 ? op_ext_xsum(op) : op_mem(op);
            default:  return false;  // DXYN display belongs to ch05
        }
    }
};

}  // namespace chip8
