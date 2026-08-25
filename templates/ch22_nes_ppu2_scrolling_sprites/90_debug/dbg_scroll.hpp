#pragma once
#include <cstdint>

// 90_debug — a copy of the loopy latch logic with ONE seeded defect.
// Skeleton side carries the bug; solution side is correct.
// Symptoms are documented in DEBUGGING.md.
namespace nes22dbg {

struct Loopy {
    uint16_t v = 0;
    uint16_t t = 0;
    uint8_t x = 0;
    bool w = false;
};

inline void ctrl_write(Loopy& l, uint8_t data) {
    l.t = uint16_t((l.t & 0x73FF) | ((data & 0x03) << 10));
}

// $2005 write, two halves via the w toggle.
//
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void scroll_write(Loopy& l, uint8_t data) {
    if (!l.w) {
        l.x = data & 0x07;
        l.t = uint16_t((l.t & ~0x001Fu) | (data >> 3));
        l.w = true;
    } else {
        l.t = uint16_t((l.t & ~0x73E0u) | ((data & 0x07) << 12)
                       | ((data & 0xF8) << 2));
        l.w = false;
    }
}
//@LABS-STUB
// Seeded defect: find it by comparing against hardware behavior — do not
// rewrite from scratch. One token/expression is wrong.
inline void scroll_write(Loopy& l, uint8_t data) {
    if (!l.w) {
        l.x = data & 0x07;
        l.t = uint16_t((l.t & ~0x001Fu) | (data >> 3));
        l.w = true;
    } else {
        l.t = uint16_t((l.t & ~0x73E0u) | (data & 0x07)
                       | ((data & 0xF8) << 2));
        l.w = false;
    }
}
//@LABS-END

inline void addr_write(Loopy& l, uint8_t data) {
    if (!l.w) {
        l.t = uint16_t((l.t & 0x00FF) | ((data & 0x3F) << 8));
        l.w = true;
    } else {
        l.t = uint16_t((l.t & 0x7F00) | data);
        l.v = l.t;
        l.w = false;
    }
}

inline void status_read(Loopy& l) { l.w = false; }

}  // namespace nes22dbg
