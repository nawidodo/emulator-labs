#pragma once
#include <cstdint>

// Chapter 22 — the "loopy" internal VRAM address registers, EXACT model.
//
// The PPU holds two 15-bit registers plus two small latches:
//
//   t: transitory address — the target the CPU writes through $2005/$2006
//   v: current VRAM address — what $2007 actually accesses, and what the
//      rendering pipeline reads from dot to dot
//   x: fine X scroll (3 bits), latched on the FIRST $2005 write
//   w: write toggle (first/second write), shared by $2005/$2006,
//      cleared by reading $2002
//
// Bit layout of v and t:
//   0yyy NNYY YYYX XXXX
//    ||| |||| ||++--- coarse X (0-4)
//    ||| ++++-------- coarse Y (5-9)
//    +++------------- nametable (10-11)
//    (bit 15 unused, always clear)
//   fine Y lives in bits 12-14 ("y" above), fine X only in the x latch.
namespace nes22scroll {

struct Loopy {
    uint16_t v = 0;
    uint16_t t = 0;
    uint8_t x = 0;
    bool w = false;
};

// $2000 write: only the base nametable select (bits 0-1) is copied into t.
//
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void ctrl_write(Loopy& l, uint8_t data) {
    l.t = uint16_t((l.t & 0x73FF) | ((data & 0x03) << 10));
}
//@LABS-STUB
// TODO(1): copy data bits 0-1 into t bits 10-11, preserving everything else.
inline void ctrl_write(Loopy& /*l*/, uint8_t /*data*/) {
    // wrong on purpose: t unchanged
}
//@LABS-END

// $2005 write, two halves selected by the w toggle.
//   1st: fine X -> x latch, coarse X -> t bits 0-4
//   2nd: fine Y -> t bits 12-14, coarse Y -> t bits 5-9
//
//@LABS-BEGIN 2
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
// TODO(2): implement both halves exactly as the header comment describes,
// including toggling w. The stub leaves t untouched (wrong on purpose).
inline void scroll_write(Loopy& /*l*/, uint8_t /*data*/) {
    // wrong on purpose
}
//@LABS-END

// $2006 write, two halves; $2002 read clears the toggle.
//   1st: high 6 bits -> t bits 8-13 (bit 14 of t is forced clear)
//   2nd: low 8 bits -> t bits 0-7, then t is copied into v
//
//@LABS-BEGIN 3
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(3): implement addr_write (both halves, copying t into v at the end)
// and status_read (clears w). Stub does nothing (wrong on purpose).
inline void addr_write(Loopy& /*l*/, uint8_t /*data*/) {}

inline void status_read(Loopy& /*l*/) {}
//@LABS-END

// $2007 read/write side effect: v increments by 1, or by 32 when PPUCTRL
// bit 2 is set (used by software for column/row updates). v is 15 bits.
//
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline void data_access(Loopy& l, uint8_t ctrl) {
    l.v = uint16_t((l.v + ((ctrl & 0x04) ? 32 : 1)) & 0x7FFF);
}
//@LABS-STUB
// TODO(4): bump v by 32 when ctrl bit 2 set, else by 1; wrap at 15 bits.
inline void data_access(Loopy& l, uint8_t /*ctrl*/) {
    l.v = uint16_t((l.v + 1) & 0x7FFF);  // wrong: ignores ctrl bit 2
}
//@LABS-END

// Coarse increments performed by the rendering pipeline during fetch dots.
//
//@LABS-BEGIN 5
//@LABS-SOLUTION
inline void increment_x(Loopy& l) {
    if ((l.v & 0x001F) == 31) {
        l.v &= ~0x001Fu;
        l.v ^= 0x0400;              // wrap into the other nametable column
    } else {
        l.v += 1;
    }
}

inline void increment_y(Loopy& l) {
    if ((l.v & 0x7000) != 0x7000) {
        l.v += 0x1000;              // fine Y not exhausted yet
    } else {
        l.v &= ~0x7000u;
        uint16_t y = (l.v >> 5) & 0x1F;
        if (y == 29) {
            y = 0;
            l.v ^= 0x0800;          // vertical nametable flip
        } else if (y == 31) {
            y = 0;                  // attribute row: no nametable flip
        } else {
            y += 1;
        }
        l.v = uint16_t((l.v & ~0x03E0u) | (y << 5));
    }
}
//@LABS-STUB
// TODO(5): implement both coarse increments exactly (see comments):
// increment_x wraps coarse X 31->0 while flipping nametable bit 10;
// increment_y counts fine Y down from 7, wraps coarse Y 29->0 flipping
// nametable bit 11, treats coarse Y 31 as transparent, else increments.
inline void increment_x(Loopy& /*l*/) {}
inline void increment_y(Loopy& /*l*/) {}
//@LABS-END

// Copies at dot 256 / 257 / pre-render line: reload parts of v from t.
//
//@LABS-BEGIN 6
//@LABS-SOLUTION
inline void copy_x(Loopy& l) {
    l.v = uint16_t((l.v & ~0x041Fu) | (l.t & 0x041Fu));
}

inline void copy_y(Loopy& l) {
    l.v = uint16_t((l.v & ~0x7BE0u) | (l.t & 0x7BE0u));
}
//@LABS-STUB
// TODO(6): copy_x moves coarse X + horizontal nametable bit (mask 0x041F)
// from t into v; copy_y moves fine Y + coarse Y + vertical nametable bits
// (mask 0x7BE0). Stubs leave v alone (wrong on purpose).
inline void copy_x(Loopy& /*l*/) {}
inline void copy_y(Loopy& /*l*/) {}
//@LABS-END

}  // namespace nes22scroll
