#pragma once
#include <cstdint>

// The tiny synthetic RISC core used by this chapter's stand-in SoC.
//
// It exists so boot programs are REAL assembled code (committed as .bin +
// .asm.txt) rather than magic test hooks. Fixed 4-cycle instruction cost
// keeps every trace and event log exactly reproducible.
//
// Word encoding (32-bit little-endian in .bin files):
//   op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31;
//   imm16 = w & 0xFFFF (sign-extended where noted)
//   ops: 0 LUI   rt = imm << 16
//        1 ORI   rt |= imm (zero-extended)
//        2 ADDIU rt = rs + simm          (r0 stays 0)
//        3 SW    mem[r[rs]+simm] = r[rt]
//        4 LW    r[rt] = mem[r[rs]+simm]
//        5 BNEZ  pc = r[rs]!=0 ? pc+1+offs : pc+1   (offs = signext imm16)
//        6 J     pc = w & 0x3FFFFFF                 (absolute word index)
//        7 HALT  stop fetching
//        8 ANDI  rt = rs & imm (zero-extended)
//
// `pc` counts WORDS (byte address = pc*4). Memory access goes through a
// Bus so the same decoder serves RAM, MMIO registers, and tests.
namespace ps1sys {

constexpr unsigned kInstrCycles = 4;

struct Bus {
    virtual ~Bus() = default;
    virtual uint32_t load32(uint32_t addr) = 0;
    virtual void store32(uint32_t addr, uint32_t val) = 0;
};

struct Core {
    static constexpr unsigned kNumRegs = 32;

    uint32_t pc = 0;               // word index
    uint32_t r[kNumRegs] = {};
    bool halted = false;

    void reset() {
        pc = 0;
        for (auto& v : r) v = 0;
        halted = false;
    }

    // Decode and execute one instruction at pc against `bus`. Returns the
    // raw instruction word (for tracing). Branches/jumps set pc directly.
    uint32_t step(Bus& bus) {
        const uint32_t w = bus.load32(pc * 4u);
        const unsigned op = w >> 26;
        const unsigned rs = (w >> 21) & 31;
        const unsigned rt = (w >> 16) & 31;
        const uint32_t imm = w & 0xFFFFu;
        const int32_t simm = static_cast<int32_t>(static_cast<int16_t>(imm));
        uint32_t next = pc + 1;
        switch (op) {
            case 0: set(rt, imm << 16); break;                       // LUI
            case 1: set(rt, r[rt] | imm); break;                     // ORI
            case 2: set(rt, r[rs] + static_cast<uint32_t>(simm)); break;
            case 3: bus.store32(r[rs] + static_cast<uint32_t>(simm),
                                r[rt]); break;                        // SW
            case 4: set(rt, bus.load32(r[rs] +
                                       static_cast<uint32_t>(simm))); break;
            case 5:                                                  // BNEZ
                if (r[rs] != 0)
                    next = static_cast<uint32_t>(
                        static_cast<int64_t>(pc) + 1 + simm);
                break;
            case 6: next = w & 0x3FFFFFFu; break;                    // J
            case 7: halted = true; break;                            // HALT
            case 8: set(rt, r[rs] & imm); break;                     // ANDI
            default: halted = true; break;  // undefined opcode halts
        }
        pc = next;
        return w;
    }

private:
    void set(unsigned idx, uint32_t val) {
        if (idx != 0) r[idx] = val;  // r0 is hardwired to zero
    }
};

}  // namespace ps1sys
