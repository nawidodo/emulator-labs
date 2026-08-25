// 65C816 register file and width/emulation-mode switching.
//
// The 65C816 is a 16-bit CPU whose registers are *width-switchable at
// runtime*. The status flag M (P bit 5) selects 8/16-bit accumulator
// width; flag X (P bit 4) selects 8/16-bit index width. Both flags only
// exist in native mode: in emulation mode (E=1) the CPU pretends to be
// a 6502, which forces M=X=1 and clears the high bytes of X and Y.
//
// Status byte layout: N V M X D I Z C (bit 7 .. bit 0).
#pragma once

#include <cstdint>

namespace snescpu {

enum : uint8_t {
    FC = 1 << 0,  // carry
    FZ = 1 << 1,  // zero
    FI = 1 << 2,  // IRQ disable
    FD = 1 << 3,  // decimal mode
    FX = 1 << 4,  // index registers 8-bit
    FM = 1 << 5,  // accumulator 8-bit
    FV = 1 << 6,  // overflow
    FN = 1 << 7,  // negative
};

struct Regs {
    uint16_t a = 0;   // full 16-bit accumulator (C); low byte is A
    uint16_t x = 0;   // X index (high byte forced 0 in emulation)
    uint16_t y = 0;   // Y index (high byte forced 0 in emulation)
    uint8_t  db = 0;  // data bank register (B): default bank for abs addressing
    uint8_t  k  = 0;  // program bank register (K): bank the PC executes in
    uint16_t d  = 0;  // direct page register (D): base for dp addressing
    uint16_t sp = 0;  // stack pointer
    uint16_t pc = 0;  // program counter (offset inside bank K)
    uint8_t  p = FI | FM | FX;  // status flags
    bool     e = true;            // emulation mode flag (outside P)
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint8_t pack_p(const Regs& r) {
    return r.p;
}

inline void unpack_p(Regs& r, uint8_t value) {
    r.p = value;
}
//@LABS-STUB
// TODO(1): implement P-byte packing/unpacking. The P byte is simply the
// stored flag byte; these accessors exist so later exercises have one
// canonical spelling. Stub returns 0 so tests run RED.
inline uint8_t pack_p(const Regs& r) {
    (void)r;
    return 0;
}
inline void unpack_p(Regs& r, uint8_t value) {
    (void)r;
    (void)value;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void sep(Regs& r, uint8_t mask) {
    r.p |= mask;
    if (r.e) {
        // In emulation mode M and X are hard-wired to 1: SEP/REP writes
        // to those bits are ignored.
        r.p |= FM | FX;
    }
}

inline void rep(Regs& r, uint8_t mask) {
    r.p &= uint8_t(~mask);
    if (r.e) {
        r.p |= FM | FX;
    }
}
//@LABS-STUB
// TODO(2): implement SEP/REP. sep() sets every flag bit in `mask`,
// rep() clears them. In emulation mode (r.e) bits FM and FX must stay
// set regardless of mask. Stubs below are wrong on purpose.
inline void sep(Regs& r, uint8_t mask) {
    (void)r;
    (void)mask;
}

inline void rep(Regs& r, uint8_t mask) {
    (void)r;
    (void)mask;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void xce(Regs& r) {
    const bool old_e = r.e;
    r.e = (r.p & FC) != 0;
    if (old_e != r.e && r.e) {
        // Entering emulation mode: M/X forced 8-bit and XH/YH cleared,
        // per WDC datasheet. Leaving emulation mode leaves widths as
        // the hidden M/X flags had them (reset state forces 8-bit).
        r.p |= FM | FX;
        r.x &= 0x00FF;
        r.y &= 0x00FF;
    }
    r.p = uint8_t((r.p & ~FC) | (old_e ? FC : 0));
}
//@LABS-STUB
// TODO(3): implement XCE (exchange carry and emulation bits). New E =
// old carry; new carry = old E. When switching INTO emulation mode the
// high bytes of X and Y are cleared and FM|FX are forced on. Switching
// to native mode changes nothing else. Stub does nothing (wrong).
inline void xce(Regs&) {
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline bool a_is_8bit(const Regs& r) {
    return r.e || (r.p & FM) != 0;
}

inline bool xy_is_8bit(const Regs& r) {
    return r.e || (r.p & FX) != 0;
}

inline uint16_t a_mask(const Regs& r) {
    return a_is_8bit(r) ? 0x00FFu : 0xFFFFu;
}

inline uint16_t xy_mask(const Regs& r) {
    return xy_is_8bit(r) ? 0x00FFu : 0xFFFFu;
}
//@LABS-STUB
// TODO(4): implement width queries. The accumulator is 8-bit when in
// emulation mode or when FM is set; same rule for X/Y with FX. The
// *_mask helpers return the AND-mask that reduces a 16-bit register to
// its usable width. Stubs claim everything is 16-bit (wrong).
inline bool a_is_8bit(const Regs&) {
    return false;
}

inline bool xy_is_8bit(const Regs&) {
    return false;
}

inline uint16_t a_mask(const Regs& r) {
    (void)r;
    return 0xFFFFu;
}

inline uint16_t xy_mask(const Regs& r) {
    (void)r;
    return 0xFFFFu;
}
//@LABS-END

}  // namespace snescpu
