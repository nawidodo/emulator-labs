#pragma once
#include <cstdint>

#include "bus.hpp"

// 8080 stack mechanics.
//
// The 8080 stack grows DOWNWARD. PUSH pre-decrements SP twice storing
// high byte at SP-1 and low byte at SP-2; POP post-increments. PUSH/POP
// take 11/10 T-states respectively. PSW pushes as A (high) + flags (low),
// with bit 1 always set and bits 3/5 always clear.

namespace i8080 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void push(Bus& bus, uint16_t& sp, uint16_t value) {
    bus.write(uint16_t(sp - 1), uint8_t(value >> 8));
    bus.write(uint16_t(sp - 2), uint8_t(value));
    sp = uint16_t(sp - 2);
}
//@LABS-STUB
// TODO(1): push high byte at SP-1, low byte at SP-2; SP -= 2.
inline void push(Bus&, uint16_t&, uint16_t) {}  // no-op: tests fail
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint16_t pop(const Bus& bus, uint16_t& sp) {
    const uint8_t lo = bus.read(sp);
    const uint8_t hi = bus.read(uint16_t(sp + 1));
    sp = uint16_t(sp + 2);
    return uint16_t(uint16_t(hi) << 8 | lo);
}
//@LABS-STUB
// TODO(2): pop low byte from SP, high byte from SP+1; SP += 2.
inline uint16_t pop(const Bus&, uint16_t&) {
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// PSW low byte layout pushed by PUSH PSW: S Z 0 AC 0 P 1 CY.
inline uint8_t pack_psw(bool s, bool z, bool ac, bool p, bool cy) {
    return uint8_t((s ? FLAG_S : 0) | (z ? FLAG_Z : 0) |
                   (ac ? FLAG_AC : 0) | (p ? FLAG_P : 0) |
                   (cy ? FLAG_CY : 0) | 0x02);
}
//@LABS-STUB
// TODO(3): pack flags into PSW order with bit 1 always set.
inline uint8_t pack_psw(bool, bool, bool, bool, bool) {
    return 0;  // wrong on purpose
}
//@LABS-END

// Decoded PSW view (fixed struct so dependent code always compiles).
struct FlagsView {
    bool s, z, ac, p, cy;
};

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline FlagsView unpack_psw(uint8_t f) {
    return {(f & FLAG_S) != 0,
            (f & FLAG_Z) != 0,
            (f & FLAG_AC) != 0,
            (f & FLAG_P) != 0,
            (f & FLAG_CY) != 0};
}
//@LABS-STUB
// TODO(4): decode PSW byte back into individual flags.
inline FlagsView unpack_psw(uint8_t) {
    return {false, false, false, false, false};  // wrong on purpose
}
//@LABS-END

}  // namespace i8080
