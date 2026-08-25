// 65C816 24-bit addressing: a 16-bit CPU addressing 16 MB through bank
// registers. Every effective address is (bank, offset). Which register
// supplies the bank is mode-specific:
//
//   dp            -> bank $00 always (direct page lives in bank zero)
//   abs           -> DB register
//   abs,X / abs,Y -> DB register, wraps INSIDE the bank (no bank cross)
//   long          -> 24-bit operand, ignores DB entirely
//   stack relative-> bank $00 always (stack lives in bank zero)
//
// Index-width matters: when X/Y are 8-bit they act as 0..255 and cannot
// cause page crossings beyond what an 8-bit add can produce.
#pragma once

#include <cstdint>

namespace snescpu {

struct CpuState {
    uint8_t  db = 0;
    uint8_t  k = 0;
    uint16_t d = 0;
    uint16_t sp = 0x01FF;
    bool     xy_8bit = true;   // FX flag equivalent
};

struct Ea {
    uint8_t  bank = 0;
    uint16_t addr = 0;
    bool     page_crossed = false;  // extra-cycle penalty flag
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint16_t dp_effective(const CpuState& c, uint8_t off, uint16_t index) {
    const uint16_t base = uint16_t(c.d + off);
    return uint16_t(base + index);
}

inline bool dp_page_crossed(const CpuState& c, uint8_t off, uint16_t index) {
    // Penalty fires when adding the FULL index changes the high byte of
    // the direct-page address (same rule as abs,index).
    const uint16_t base = uint16_t(c.d + off);
    return (base & 0xFF00u) != (uint16_t(base + index) & 0xFF00u);
}
//@LABS-STUB
// TODO(1): implement direct-page effective address. The address is
// (D + off + index) truncated to 16 bits; it always lives in bank $00.
// dp_page_crossed reports whether adding the index crosses a page
// boundary (the hardware charges +1 cycle for that).
// Stubs are wrong on purpose.
inline uint16_t dp_effective(const CpuState& c, uint8_t off, uint16_t index) {
    (void)c;
    (void)off;
    (void)index;
    return 0;
}

inline bool dp_page_crossed(const CpuState&, uint8_t, uint16_t) {
    return false;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint32_t abs_effective(const CpuState& c, uint16_t addr) {
    return (uint32_t(c.db) << 16) | addr;
}

inline uint32_t abs_indexed(const CpuState& c, uint16_t addr,
                            uint16_t index, bool* crossed) {
    const uint16_t sum = uint16_t(addr + index);  // wraps inside the bank
    if (crossed) *crossed = (addr & 0xFF00u) != (sum & 0xFF00u);
    return (uint32_t(c.db) << 16) | sum;
}

inline uint32_t long_effective(uint32_t addr24) {
    return addr24 & 0x00FFFFFFu;  // operand supplies its own bank
}
//@LABS-STUB
// TODO(2): absolute and long addressing. abs puts the 16-bit operand at
// offset `addr` inside the DB bank. abs-indexed adds the index with
// 16-bit wrap-around INSIDE the bank and reports whether the high byte
// of the offset changed (+1 cycle). long returns the 24-bit operand as
//-is (masked), ignoring DB. Stubs are wrong on purpose.
inline uint32_t abs_effective(const CpuState& c, uint16_t addr) {
    (void)c;
    (void)addr;
    return 0;
}

inline uint32_t abs_indexed(const CpuState& c, uint16_t addr,
                            uint16_t index, bool* crossed) {
    (void)c;
    (void)addr;
    (void)index;
    if (crossed) *crossed = false;
    return 0;
}

inline uint32_t long_effective(uint32_t addr24) {
    (void)addr24;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint16_t sr_effective(const CpuState& c, uint8_t off) {
    return uint16_t(c.sp + off);  // stack-relative: bank $00, no wrap penalty
}
//@LABS-STUB
// TODO(3): stack-relative addressing adds a signed-in-practice small
// offset to SP; result stays in bank $00 and truncates to 16 bits.
inline uint16_t sr_effective(const CpuState& c, uint8_t off) {
    (void)c;
    (void)off;
    return 0;
}
//@LABS-END

}  // namespace snescpu
