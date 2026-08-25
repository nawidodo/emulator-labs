// CODING TEST target: LDA (dp),Y — post-indexed indirect, opcode $B7.
// This mode was NOT implemented in the exercises; you implement it
// here from SPEC.md alone. Semantics in one glance:
//
//   ptr := word at bank $00, address (D + dp_offset)   [the pointer]
//   ea := (ptr + Y) truncated to 16 bits               [Y full width]
//   bank := DB                                          [not bank 0!]
//   cycles := 5 (+1 when adding Y crosses a page of `ptr`)
//
// Width rules are identical to every other LDA: read A-width bytes,
// preserve hidden B on 8-bit loads, update Z/N for the current width.
#pragma once

#include <cstdint>

namespace snescpu {

enum : uint8_t {
    FC = 1 << 0, FZ = 1 << 1, FI = 1 << 2, FD = 1 << 3,
    FX = 1 << 4, FM = 1 << 5, FV = 1 << 6, FN = 1 << 7,
};

struct Cpu {
    uint16_t a = 0, x = 0, y = 0;
    uint8_t db = 0, k = 0;
    uint16_t d = 0, sp = 0x01FF, pc = 0;
    uint8_t p = FI | FM | FX;
    bool e = true;
};

inline bool a_is_8bit(const Cpu& c) { return c.e || (c.p & FM) != 0; }
inline bool xy_is_8bit(const Cpu& c) { return c.e || (c.p & FX) != 0; }
inline uint16_t xy_mask(const Cpu& c) {
    return xy_is_8bit(c) ? 0x00FFu : 0xFFFFu;
}

struct Mem {
    // Registered bank images (64 KiB each); fixtures register the
    // banks they use before execution.
    const uint8_t* banks[256] = {};
    uint8_t read(uint8_t b, uint16_t addr) const {
        return banks[b][addr];
    }
    uint16_t read16(uint8_t b, uint16_t addr) const {
        return uint16_t(read(b, addr)) |
               uint16_t(read(b, uint16_t(addr + 1))) << 8;
    }
};

inline uint8_t fetch8(Cpu& c, const Mem& m) {
    const uint8_t v = m.read(c.k, c.pc);
    ++c.pc;
    return v;
}

inline void set_zn_a(Cpu& c, uint16_t value) {
    if (a_is_8bit(c)) value &= 0x00FFu;
    c.p &= uint8_t(~(FZ | FN));
    if ((value & (a_is_8bit(c) ? 0xFFu : 0xFFFFu)) == 0) c.p |= FZ;
    if (value & (a_is_8bit(c) ? 0x0080u : 0x8000u)) c.p |= FN;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int lda_dp_indirect_y(Cpu& c, const Mem& m, uint8_t dp_off,
                             bool* page_crossed) {
    // 1) pointer lives in BANK ZERO at (D + offset)
    const uint16_t ptr = m.read16(0x00, uint16_t(c.d + dp_off));
    // 2) add Y at FULL width, wrapping inside the DB bank
    const uint16_t idx = c.y & xy_mask(c);
    const uint16_t sum = uint16_t(ptr + idx);
    if (page_crossed != nullptr) {
        *page_crossed =
            (ptr & 0xFF00u) != (sum & 0xFF00u);
    }
    // 3) the DATA is fetched from the DB bank, not from bank zero
    if (a_is_8bit(c)) {
        c.a = uint16_t((c.a & 0xFF00u) | m.read(c.db, sum));
        set_zn_a(c, c.a);
    } else {
        c.a = m.read16(c.db, sum);
        set_zn_a(c, c.a);
    }
    return 5;
}
//@LABS-STUB
// TODO(1): implement LDA (dp),Y exactly as SPEC.md describes. The stub
// below skips the indirection entirely and always reports no page
// cross — wrong on purpose but it compiles so the suite runs.
// Return value: base cycle count (5). Set *page_crossed when adding Y
// moves `ptr` into a different 256-byte page.
inline int lda_dp_indirect_y(Cpu& c, const Mem& m, uint8_t dp_off,
                             bool* page_crossed) {
    (void)c;
    (void)m;
    (void)dp_off;
    if (page_crossed != nullptr) *page_crossed = false;
    return 5;
}
//@LABS-END

}  // namespace snescpu
