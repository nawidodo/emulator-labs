#pragma once
#include <cstdint>

namespace arm {

// CPSR flag bits.
constexpr uint32_t FLAG_N = 1u << 31;
constexpr uint32_t FLAG_Z = 1u << 30;
constexpr uint32_t FLAG_C = 1u << 29;
constexpr uint32_t FLAG_V = 1u << 28;

// Every ARM instruction carries a 4-bit condition field; the instruction
// executes only when its predicate holds (see LECTURE.md table). We split
// the table into three families so each can be tested exhaustively.

// Equality + unsigned family. `cond` is one of:
//   0000 EQ, 0001 NE, 0010 CS, 0011 CC, 1000 HI, 1001 LS
// Anything else returns false.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline bool cond_unsigned(uint32_t cond, bool z, bool c) {
    switch (cond) {
    case 0x0: return z;             // EQ
    case 0x1: return !z;            // NE
    case 0x2: return c;             // CS / HS
    case 0x3: return !c;            // CC / LO
    case 0x8: return c && !z;       // HI
    case 0x9: return !c || z;       // LS
    default:  return false;
    }
}
//@LABS-STUB
inline bool cond_unsigned(uint32_t cond, bool z, bool c) {
    // TODO(1): EQ/NE from Z, CS/CC from C, HI = C && !Z, LS = !C || Z.
    (void)cond; (void)z; (void)c;
    return false;
}
//@LABS-END

// Signed family: 1010 GE, 1011 LT, 1100 GT, 1101 LE.
// Signed comparisons compare N against V (overflow flips perceived sign).
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline bool cond_signed(uint32_t cond, bool n, bool v, bool z) {
    switch (cond) {
    case 0xA: return n == v;          // GE
    case 0xB: return n != v;          // LT
    case 0xC: return !z && n == v;    // GT
    case 0xD: return z || n != v;     // LE
    default:  return false;
    }
}
//@LABS-STUB
inline bool cond_signed(uint32_t cond, bool n, bool v, bool z) {
    // TODO(2): GE/LT test N == V, GT/LE fold in Z first.
    (void)cond; (void)n; (void)v; (void)z;
    return false;
}
//@LABS-END

// Misc group: 0100 MI, 0101 PL, 0110 VS, 0111 VC, 1110 AL, 1111 NV.
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline bool cond_misc(uint32_t cond, bool n, bool v) {
    switch (cond) {
    case 0x4: return n;      // MI
    case 0x5: return !n;     // PL
    case 0x6: return v;      // VS
    case 0x7: return !v;     // VC
    case 0xE: return true;   // AL
    default:  return false;  // NV never executes
    }
}
//@LABS-STUB
inline bool cond_misc(uint32_t cond, bool n, bool v) {
    // TODO(3): MI/PL from N, VS/VC from V, AL always, NV never.
    (void)cond; (void)n; (void)v;
    return false;
}
//@LABS-END

// Full predicate: decode the 4-bit field and route to the right family.
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline bool cond_pass(uint32_t cond, uint32_t cpsr) {
    const bool n = cpsr & FLAG_N;
    const bool z = cpsr & FLAG_Z;
    const bool c = cpsr & FLAG_C;
    const bool v = cpsr & FLAG_V;
    if (cond <= 0x3 || cond == 0x8 || cond == 0x9)
        return cond_unsigned(cond, z, c);
    if (cond >= 0xA && cond <= 0xD)
        return cond_signed(cond, n, v, z);
    return cond_misc(cond, n, v);
}
//@LABS-STUB
inline bool cond_pass(uint32_t cond, uint32_t cpsr) {
    // TODO(4): route the 4-bit field to cond_unsigned / cond_signed /
    // cond_misc using the flag bits of cpsr.
    (void)cond; (void)cpsr;
    return true;
}
//@LABS-END

}  // namespace arm
