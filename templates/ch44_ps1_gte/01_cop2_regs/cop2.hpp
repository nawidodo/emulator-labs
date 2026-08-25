#pragma once
//
// ch44 / 01_cop2_regs — GTE COP2 register model, fixed-point conventions
// and command-word decode (psx-spx "Geometry Transformation Engine").
//
// Fixed-point conventions used throughout chapter 44:
//   * input vectors VX/VY/VZ : 1.3.12 signed (-32768..32767 counts)
//   * matrix elements        : 1.3.12 (4096 == 1.0)
//   * translation TRX/TRZ    : 1.19.12 (sign-extended 32-bit)
//   * IR0..IR3               : 1.19.12 stored in int16 (saturating)
//   * MAC0..MAC3             : wide intermediates (we track 44-bit sums)
//
// Architectural saturation (what the register file exposes) is separated
// from mathematical width (the int64 accumulators) — curriculum solution
// note for this phase.

#include <cstdint>

namespace gte {

struct Command {
    unsigned op;   // bits 5..0  (RTPS=01h, RTPT=02h, NCDS=05h, MVMVA=12h,
                   //             AVSZ3=1Bh, NCLIP=04h)
    bool sf;       // bit 10     1 => shift products right by 12
    bool lm;       // bit 11     1 => clamp IR to unsigned range
    unsigned mat;  // MVMVA bits 17..16: matrix select
    unsigned vec;  // MVMVA bits 15..14: vector select
    bool add12;    // MVMVA bit 13 CLEAR => add translation/background
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline Command decode_command(uint32_t word) {
    Command c;
    c.op = word & 0x3Fu;
    c.sf = ((word >> 10) & 1u) != 0;
    c.lm = ((word >> 11) & 1u) != 0;
    c.mat = (word >> 16) & 0x3u;
    c.vec = (word >> 14) & 0x3u;
    c.add12 = ((word >> 13) & 1u) == 0;
    return c;
}
//@LABS-STUB
// TODO(1): decode opcode (bits 5..0), SF (bit 10), LM (bit 11) and the
// MVMVA selection fields (matrix 17..16, vector 15..14, add 13 where a
// ZERO bit means "add").
Command decode_command(uint32_t word) {
    (void)word;
    return Command{0, false, false, 0, 0, false};  // wrong on purpose
}
//@LABS-END

// FLAG bits written by arithmetic ops (lower 16 bits mirror the upper).
enum FlagBits : uint32_t {
    kFlagIrSatUnsigned = 1u << 27,  // IR1..3 clamped to 0..32767 (LM=1)
    kFlagIrSatSigned   = 1u << 28,  // IR1..3 clamped to -32768..32767
    kFlagMac0NegOvf    = 1u << 20,  // MAC0 out of signed 32-bit range
    kFlagMac0PosOvf    = 1u << 21,
    kFlagMacNegOvf     = 1u << 29,  // MAC1..3 44-bit accumulator overflow
    kFlagMacPosOvf     = 1u << 30,
    kFlagDivideOvf     = 1u << 31,  // RTPS divide by SZ3==0
    kFlagLmEcho        = 1u << 16,
    kFlagSfEcho        = 1u << 17,
    kFlagError         = 1u << 31,  // aggregate: OR of every error bit
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint32_t compose_flag(uint32_t bits, bool sf, bool lm) {
    uint32_t f = bits & 0xFF9F0000u;      // keep defined bits incl. DIV
    const uint32_t errors =
        (1u << 27) | (1u << 28) | (1u << 20) | (1u << 21) |
        (1u << 29) | (1u << 30) | (1u << 31);
    if (f & errors) f |= kFlagError;
    if (sf) f |= kFlagSfEcho;
    if (lm) f |= kFlagLmEcho;
    return f | (f >> 16);                 // lower half mirrors upper
}

// Architectural saturation of an IR lane. `flags` accumulates sticky bits
// for the current command.
inline int16_t saturate_ir(int64_t v, bool lm, uint32_t& flags) {
    if (lm) {
        if (v < 0)          { flags |= kFlagIrSatUnsigned; return 0; }
        if (v > 32767)      { flags |= kFlagIrSatUnsigned; return 32767; }
        return static_cast<int16_t>(v);
    }
    if (v < -32768)     { flags |= kFlagIrSatSigned; return -32768; }
    if (v > 32767)      { flags |= kFlagIrSatSigned; return 32767; }
    return static_cast<int16_t>(v);
}
//@LABS-STUB
// TODO(2): implement FLAG composition (error aggregation, SF/LM echoes,
// mirrored lower half) and saturate_ir() honoring LM: LM=1 clamps to
// 0..32767 and raises bit 27, LM=0 clamps to -32768..32767 raising bit 28.
uint32_t compose_flag(uint32_t bits, bool sf, bool lm) {
    (void)bits; (void)sf; (void)lm;
    return 0;  // wrong on purpose
}
int16_t saturate_ir(int64_t v, bool lm, uint32_t& flags) {
    (void)v; (void)lm; (void)flags;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
class Cop2 {
public:
    // Data space: 32 words. Only the ones this chapter touches get named
    // accessors; everything else is opaque R/W.
    uint32_t rd(unsigned idx) const { return d_[idx & 31]; }
    void wd(unsigned idx, uint32_t v) { d_[idx & 31] = v; }

    int16_t vx(unsigned slot) const {
        return static_cast<int16_t>(rd(slot * 2) & 0xFFFFu);
    }
    int16_t vy(unsigned slot) const {
        return static_cast<int16_t>(rd(slot * 2) >> 16);
    }
    int16_t vz(unsigned slot) const {
        return static_cast<int16_t>(rd(slot * 2 + 1));
    }

    uint32_t cr(unsigned idx) const { return c_[idx & 31]; }
    void wc(unsigned idx, uint32_t v) { c_[idx & 31] = v; }

    // Control space: matrix elements pack two 1.3.12 lanes per word,
    // low half first (R11R12 share control word 0, and so on).
    int16_t rot(unsigned row, unsigned col) const {
        const unsigned idx = row * 3 + col;
        const uint32_t w = c_[idx / 2];
        return static_cast<int16_t>(idx % 2 == 0 ? (w & 0xFFFFu)
                                                  : (w >> 16));
    }
    void set_rot(unsigned row, unsigned col, int16_t v) {
        const unsigned idx = row * 3 + col;
        const uint32_t mask = idx % 2 == 0 ? 0x0000FFFFu : 0xFFFF0000u;
        c_[idx / 2] = (c_[idx / 2] & ~mask) |
                      (static_cast<uint32_t>(static_cast<uint16_t>(v))
                       << (idx % 2 == 0 ? 0 : 16));
    }

    int32_t tr(unsigned axis) const {
        return static_cast<int32_t>(c_[5 + axis]);
    }
    void set_tr(unsigned axis, int32_t v) { c_[5 + axis] = v & 0xFFFFFFFFu; }

    uint32_t flag() const { return c_[31]; }
    void set_flag(uint32_t v) { c_[31] = v; }
    void set_ofx(int32_t v) { c_[24] = v & 0xFFFFFFFFu; }
    void set_ofy(int32_t v) { c_[25] = v & 0xFFFFFFFFu; }
    void set_h(uint16_t v) { c_[26] = v; }
    int32_t ofx() const { return static_cast<int32_t>(c_[24]); }
    int32_t ofy() const { return static_cast<int32_t>(c_[25]); }
    uint16_t h() const { return static_cast<uint16_t>(c_[26]); }

private:
    uint32_t d_[32]{};
    uint32_t c_[32]{};
};
//@LABS-STUB
// TODO(3): implement the packed accessors. Matrix elements pack two
// 1.3.12 lanes per control word (low half first); translation lives in
// control words 5..7; OFX/OFY/H at 24..26; FLAG at 31.
class Cop2 {
public:
    uint32_t rd(unsigned idx) const { (void)idx; return 0; }
    void wd(unsigned idx, uint32_t v) { (void)idx; (void)v; }
    uint32_t cr(unsigned idx) const { (void)idx; return 0; }
    void wc(unsigned idx, uint32_t v) { (void)idx; (void)v; }
    int16_t vx(unsigned slot) const { (void)slot; return 0; }
    int16_t vy(unsigned slot) const { (void)slot; return 0; }
    int16_t vz(unsigned slot) const { (void)slot; return 0; }
    int16_t rot(unsigned row, unsigned col) const {
        (void)row; (void)col; return 0;
    }
    void set_rot(unsigned row, unsigned col, int16_t v) {
        (void)row; (void)col; (void)v;
    }
    int32_t tr(unsigned axis) const { (void)axis; return 0; }
    void set_tr(unsigned axis, int32_t v) { (void)axis; (void)v; }
    uint32_t flag() const { return 0; }
    void set_flag(uint32_t v) { (void)v; }
    void set_ofx(int32_t v) { (void)v; }
    void set_ofy(int32_t v) { (void)v; }
    void set_h(uint16_t v) { (void)v; }
    int32_t ofx() const { return 0; }
    int32_t ofy() const { return 0; }
    uint16_t h() const { return 0; }
};
//@LABS-END

}  // namespace gte
