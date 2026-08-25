#pragma once
#include <cstdint>

namespace arm {

// CPSR flag bits shared by the ARM and Thumb executors in this chapter.
// Self-contained copy of the ch25 conditions helpers, reduced to what a
// Thumb core needs: full condition evaluation against NZCV.
constexpr uint32_t FLAG_N = 1u << 31;
constexpr uint32_t FLAG_Z = 1u << 30;
constexpr uint32_t FLAG_C = 1u << 29;
constexpr uint32_t FLAG_V = 1u << 28;

// Equality + unsigned family: EQ NE CS CC HI LS.
inline bool cond_unsigned(uint32_t cond, bool z, bool c) {
    switch (cond & 3) {
    case 0: return z;                          // EQ
    case 1: return !z;                         // NE
    case 2: return c;                          // CS/HS
    default: return !c;                        // CC/LO
    }
}

// Signed family: GE LT GT LE — compare N against V.
inline bool cond_signed(uint32_t cond, bool n, bool v, bool z) {
    const bool ge = (n == v);
    switch (cond & 3) {
    case 0: return ge;                         // GE
    case 1: return !ge;                        // LT
    case 2: return ge && !z;                   // GT
    default: return !ge || z;                  // LE
    }
}

// Misc group: MI PL VS VC AL (+ NV never).
inline bool cond_misc(uint32_t cond, bool n, bool v) {
    switch (cond) {
    case 0x4: return n;                        // MI
    case 0x5: return !n;                       // PL
    case 0x6: return v;                        // VS
    case 0x7: return !v;                       // VC
    default: return cond == 0xE;               // AL
    }
}

// Full predicate over the 4-bit field. Thumb only ever uses AL via B, but
// the SWI/SVC path and shared code keep the whole table.
inline bool cond_pass(uint32_t cond, uint32_t cpsr) {
    const bool n = cpsr & FLAG_N, z = cpsr & FLAG_Z;
    const bool c = cpsr & FLAG_C, v = cpsr & FLAG_V;
    switch (cond) {
    case 0x0:  return z;                // EQ
    case 0x1:  return !z;               // NE
    case 0x2:  return c;                // CS/HS
    case 0x3:  return !c;               // CC/LO
    case 0x4:  return n;                // MI
    case 0x5:  return !n;               // PL
    case 0x6:  return v;                // VS
    case 0x7:  return !v;               // VC
    case 0x8:  return c && !z;          // HI
    case 0x9:  return !c || z;          // LS
    case 0xA:  return n == v;           // GE
    case 0xB:  return n != v;           // LT
    case 0xC:  return !z && (n == v);   // GT
    case 0xD:  return z || (n != v);    // LE
    default:   return cond == 0xE;      // AL (NV never executes)
    }
}

}  // namespace arm
