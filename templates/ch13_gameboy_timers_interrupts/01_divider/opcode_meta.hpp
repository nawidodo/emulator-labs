#pragma once
#include <cstdint>

namespace gb {

// Self-contained copy of the chapter-11 CPU interface, trimmed to what
// timer programs need. Original authored in ch11 (01_daa_rotates/
// opcode_meta.hpp); only the timing facts survive -- ch13 programs never
// need disassembly names, and the timer golden logs care about cycle
// counts, not mnemonics.
//
// `cycles` is the base cost (branch NOT taken); `cycles_alt` is the extra
// T-cycles when a conditional branch IS taken.

struct Instruction {
    uint8_t cycles;
    uint8_t cycles_alt;
};

inline constexpr Instruction opcode_info(uint8_t op) {
    switch (op) {
        case 0x00: return {4, 0};              // nop
        case 0x08: return {20, 0};             // ld (nn),sp
        // JR e / JR cc,e
        case 0x18: return {12, 0};
        case 0x20: case 0x28: case 0x30: case 0x38: return {8, 4};
        // JP nn / JP cc,nn
        case 0xC3: return {16, 0};
        case 0xC2: case 0xCA: case 0xD2: case 0xDA: return {12, 4};
        // LD (nn),A / LD A,(nn)
        case 0xEA: case 0xFA: return {16, 0};
        // LDH (n),A / LDH A,(n)
        case 0xE0: case 0xF0: return {12, 0};
        // PUSH rp2 / POP rp2
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: return {16, 0};
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: return {12, 0};
        // EI/DI/HALT are hook-served in this chapter; RETI pops + raises IME
        case 0xF3: case 0xFB: case 0x76: return {4, 0};
        case 0xD9: return {16, 0};
        // LD SP,HL / JP HL
        case 0xF9: return {8, 0};
        case 0xE9: return {4, 0};
        default: break;
    }

    const int x = op >> 6;
    const int y = (op >> 3) & 7;
    const int z = op & 7;

    if (x == 1) {  // LD r[y],r[z]; memory forms cost 8
        return {static_cast<uint8_t>((y == 6 || z == 6) ? 8 : 4), 0};
    }
    if (x == 2) {  // ALU A,r[z]
        return {static_cast<uint8_t>(z == 6 ? 8 : 4), 0};
    }

    const int p = (op >> 4) & 3;
    switch (op) {
        case 0x01: case 0x11: case 0x21: case 0x31: return {12, 0};  // LD rr,nn
        case 0x03: case 0x13: case 0x23: case 0x33: return {8, 0};   // INC rr
        case 0x0B: case 0x1B: case 0x2B: case 0x3B: return {8, 0};   // DEC rr
        case 0x09: case 0x19: case 0x29: case 0x39: return {8, 0};   // ADD HL,rr
        case 0x02: case 0x12: return {8, 0};                          // LD (rr),A
        case 0x0A: case 0x1A: return {8, 0};                          // LD A,(rr)
        case 0x22: case 0x32: case 0x2A: case 0x3A: return {8, 0};    // LDI/LDD
        case 0x06: case 0x0E: case 0x16: case 0x1E:
        case 0x26: case 0x2E: case 0x3E: return {8, 0};               // LD r,n
        case 0x04: case 0x0C: case 0x14: case 0x1C:
        case 0x24: case 0x2C: case 0x3C: return {4, 0};               // INC r
        case 0x05: case 0x0D: case 0x15: case 0x1D:
        case 0x25: case 0x2D: case 0x3D: return {4, 0};               // DEC r
        case 0x34: case 0x35: return {12, 0};                         // INC/DEC (HL)
        case 0x36: return {12, 0};                                    // LD (HL),n
        default:
            (void)p;
            return {4, 0};  // unimplemented: caller traps before using this
    }
}

}  // namespace gb
