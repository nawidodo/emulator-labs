#pragma once
#include <cstdint>

// Intel 8080 flag primitives.
//
// The 8080 exposes five programmer-visible flags in the PSW byte
// (pushed by PUSH PSW, bit order MSB->LSB: S Z 0 AC 0 P 1 CY):
//
//   S  sign      : copy of bit 7 of the result
//   Z  zero      : result == 0
//   AC aux-carry : carry out of bit 3 (decimal-adjust support)
//   P  parity    : SET when the result has an EVEN number of set bits
//   CY carry     : carry/borrow out of bit 7
//
// Getting AC and parity exactly right matters: DAA (chapter 7 coding test)
// consumes AC, and diagnostic ROMs probe parity exhaustively.

namespace i8080 {

enum : uint8_t {
    FLAG_CY = 0x01,
    FLAG_P  = 0x04,
    FLAG_AC = 0x10,
    FLAG_Z  = 0x40,
    FLAG_S  = 0x80,
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// True when v has an EVEN number of set bits (the 8080 sets PF in that
// case). Fold-and-xor population count: deterministic, branch-free.
inline bool even_parity(uint8_t v) {
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (v & 1) == 0;
}
//@LABS-STUB
// TODO(1): return true when v has an EVEN number of set bits.
inline bool even_parity(uint8_t v) {
    (void)v;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Build the S/Z/P portion of the PSW from an 8-bit result.
// Bits 3 and 5 stay clear here; pack_psw() handles the fixed bits.
inline uint8_t szp_bits(uint8_t v) {
    uint8_t f = 0;
    if (v & 0x80) f |= FLAG_S;
    if (v == 0) f |= FLAG_Z;
    if (even_parity(v)) f |= FLAG_P;
    return f;
}
//@LABS-STUB
// TODO(2): combine sign, zero and parity of v into PSW-positioned bits.
inline uint8_t szp_bits(uint8_t v) {
    (void)v;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Aux carry out of bit 3 for a + b + carry_in.
// The 8080 implements subtraction as a + ~b + !borrow, so the SAME
// half-sum circuit feeds AC for ADD-family and SUB-family alike once the
// caller complements the operand (see aux_carry_sub below).
inline bool aux_carry_add(uint8_t a, uint8_t b, bool carry_in) {
    return uint16_t((a & 0xF) + (b & 0xF) + (carry_in ? 1 : 0)) > 0xF;
}
//@LABS-STUB
// TODO(3): report carry out of bit 3 for a + b + carry_in.
inline bool aux_carry_add(uint8_t a, uint8_t b, bool carry_in) {
    (void)a; (void)b; (void)carry_in;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Aux carry for subtraction a - b - borrow_in, expressed as the hardware
// sees it: a + ~b + !borrow. Using the complement form (instead of a
// nibble comparison) reproduces 8080 AC results exactly, including the
// edge cases diagnostics probe (0x0F - 0x01 sets AC, 0x10 - 0x00 does not).
inline bool aux_carry_sub(uint8_t a, uint8_t b, bool borrow_in) {
    return aux_carry_add(a, uint8_t(~b), !borrow_in);
}
//@LABS-STUB
// TODO(4): report aux borrow for a - b - borrow_in via the complement sum.
inline bool aux_carry_sub(uint8_t a, uint8_t b, bool borrow_in) {
    (void)a; (void)b; (void)borrow_in;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Pack the PSW byte pushed by PUSH PSW: bit 1 is ALWAYS 1 on the 8080 and
// bits 3/5 are always 0 (they sit on a different bus line internally).
inline uint8_t pack_psw(uint8_t szpac) {
    return uint8_t(szpac | 0x02);
}
//@LABS-STUB
// TODO(5): OR in the always-set bit 1 of the PSW byte.
inline uint8_t pack_psw(uint8_t szpac) {
    (void)szpac;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace i8080
