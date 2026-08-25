#pragma once
#include <cstdint>
#include <bit>

class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};
// Chapter 7 coding test: ten unseen instructions from the spec table in
// CODING_TEST.md. Each function is a pure hardware unit so it can be
// tested without the full CPU; the hidden grader runs this same binary
// with per-instruction filters ("hidden.stax" etc).
namespace i8080ext {

//@LABS-BEGIN 1
//@LABS-SOLUTION
// STAX B/D (0x02/0x12): store A at the BC or DE address. 7 T-states.
inline void stax(Bus& bus, uint16_t pair, uint8_t a) {
    bus.write(pair, a);
}
//@LABS-STUB
// TODO(1): store A at the address held in the register pair.
inline void stax(Bus&, uint16_t, uint8_t) {}  // missing write: tests fail
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// LDAX B/D (0x0A/0x1A): load A from the BC or DE address. 7 T-states.
inline uint8_t ldax(const Bus& bus, uint16_t pair) {
    return bus.read(pair);
}
//@LABS-STUB
// TODO(2): read memory at the pair address.
inline uint8_t ldax(const Bus&, uint16_t) {
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// INX rp (0x03/0x13/0x23/0x33): 16-bit increment. NO flags change —
// a classic trap is routing it through INR twice and corrupting CY.
inline void inx(uint16_t& pair) {
    pair = uint16_t(pair + 1);
}
//@LABS-STUB
// TODO(3): 16-bit increment with zero flag side effects.
inline void inx(uint16_t&) {}  // no-op: tests fail
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// DCX rp (0x0B/0x1B/0x2B/0x3B): 16-bit decrement, also flag-free.
inline void dcx(uint16_t& pair) {
    pair = uint16_t(pair - 1);
}
//@LABS-STUB
// TODO(4): 16-bit decrement with zero flag side effects.
inline void dcx(uint16_t&) {}  // no-op: tests fail
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// DAD rp (0x09/0x19/0x29/0x39): HL += pair; ONLY carry changes,
// set from carry out of bit 15. 10 T-states.
inline bool dad(uint16_t& hl, uint16_t other) {
    const uint32_t sum = uint32_t(hl) + other;
    hl = uint16_t(sum);
    return (sum >> 16) != 0;
}
//@LABS-STUB
// TODO(5): HL += other; return carry out of bit 15 (only affected flag).
inline bool dad(uint16_t&, uint16_t) {
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
// DAA result record: new accumulator plus every flag DAA writes back.
struct DaaResult {
    uint8_t a;
    bool cy;
    bool ac;
    bool s;
    bool z;
    bool p;
};
//@LABS-STUB
// TODO(6): declare DaaResult {a, cy, ac, s, z, p}. The struct itself is
// given so dependent code compiles; your task arrives in block 7.
struct DaaResult {
    uint8_t a;
    bool cy;
    bool ac;
    bool s;
    bool z;
    bool p;
};
//@LABS-END

//@LABS-BEGIN 7
//@LABS-SOLUTION
// DAA (0x27): decimal adjust AFTER an add. The 8080 rule:
//   if AC || low nibble > 9 -> add 06 to the low nibble
//   if old CY || high nibble (of the ORIGINAL A) > 9 -> add 60, set CY
// CY is never cleared by DAA; S/Z/P come from the adjusted value.
inline DaaResult daa(uint8_t a, bool cy, bool ac) {
    uint8_t correction = 0;
    const bool fix_low = ac || (a & 0x0F) > 9;
    const bool fix_high = cy || (a >> 4) > 9;
    if (fix_low) correction |= 0x06;
    if (fix_high) correction |= 0x60;
    const uint16_t sum = uint16_t(a) + correction;
    const uint8_t res = uint8_t(sum);
    const int bits = std::popcount(unsigned(res));
    return {res,
            fix_high ? true : cy,
            fix_low,
            (res & 0x80) != 0,
            res == 0,
            (bits % 2) == 0};
}
//@LABS-STUB
// TODO(7): implement the two-nibble decimal adjust exactly as specified.
inline DaaResult daa(uint8_t, bool cy, bool) {
    return DaaResult{0};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 8
//@LABS-SOLUTION
// RLC (0x07): rotate A left; bit 7 -> both CY and A bit 0. 4 T-states.
inline bool rlc(uint8_t& a) {
    const bool new_cy = (a & 0x80) != 0;
    a = uint8_t((a << 1) | (new_cy ? 1 : 0));
    return new_cy;
}
//@LABS-STUB
// TODO(8): rotate left through bit 7 into CY and back into bit 0.
inline bool rlc(uint8_t&) { return false; }  // wrong on purpose
//@LABS-END

//@LABS-BEGIN 9
//@LABS-SOLUTION
// RRC (0x0F): rotate A right; bit 0 -> both CY and A bit 7.
inline bool rrc(uint8_t& a) {
    const bool new_cy = (a & 0x01) != 0;
    a = uint8_t((a >> 1) | (new_cy ? 0x80 : 0));
    return new_cy;
}
//@LABS-STUB
// TODO(9): rotate right through bit 0 into CY and back into bit 7.
inline bool rrc(uint8_t&) { return false; }  // wrong on purpose
//@LABS-END

//@LABS-BEGIN 10
//@LABS-SOLUTION
// RAL/RAR (0x17/0x1F): rotate through carry — CY participates as a 9th
// bit instead of wrapping back into A.
inline uint8_t ral(uint8_t a, bool cy_in, bool& cy_out) {
    cy_out = (a & 0x80) != 0;
    return uint8_t((a << 1) | (cy_in ? 1 : 0));
}

inline uint8_t rar(uint8_t a, bool cy_in, bool& cy_out) {
    cy_out = (a & 0x01) != 0;
    return uint8_t((a >> 1) | (cy_in ? 0x80 : 0));
}
//@LABS-STUB
// TODO(10): rotate through carry both directions.
inline uint8_t ral(uint8_t a, bool, bool& cy_out) {
    (void)a;
    cy_out = false;
    return 0;  // wrong on purpose
}

inline uint8_t rar(uint8_t a, bool, bool& cy_out) {
    (void)a;
    cy_out = false;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace i8080ext
