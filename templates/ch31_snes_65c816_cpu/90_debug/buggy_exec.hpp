// DEBUGGING EXERCISE HEADER (90_debug)
// Same executor as 03_execute but with ONE seeded defect on the STUB
// side of the step() block below. The SOLUTION side is correct. Read
// DEBUGGING.md, write bug-report.md, then flip your build to solution
// mode to compare.
// Minimal 65C816 executor: operand-width-explicit execution over a tiny
// subset of the ISA, plus trace/disassembler support.
//
// Design note (curriculum ch31 solution): the reference implementation
// keeps operand width EXPLICIT at every instruction execution site --
// each LDA/STA consults the current M/X flags instead of assuming a
// register size. This is what makes width switching testable: the same
// opcode decodes different immediate lengths and produces different
// flag results depending on SEP/REP history.
//
// Covered subset (documented simplification):
//   BRK(00) halts the CPU; NOP, XCE, SEP, REP,
//   LDA #/abs/abs,X/dp/dp,X/long/long,X, STA abs/dp/dp,X/long,
//   LDX #imm, JMP abs, JML long.
// Cycle counts are faithful for this subset (e.g. LDA abs = 4, +1 on
// index page crossing; SEP/REP = 3; XCE = 2). Unbacked memory reads 0.
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

namespace snescpu {

enum : uint8_t {
    FC = 1 << 0, FZ = 1 << 1, FI = 1 << 2, FD = 1 << 3,
    FX = 1 << 4, FM = 1 << 5, FV = 1 << 6, FN = 1 << 7,
};

struct Cpu {
    uint16_t a = 0, x = 0, y = 0;
    uint8_t  db = 0, k = 0;
    uint16_t d = 0, sp = 0x01FF, pc = 0;
    uint8_t  p = FI | FM | FX;
    bool     e = true;
    uint64_t cycles = 0;
};

inline bool a_is_8bit(const Cpu& c) { return c.e || (c.p & FM) != 0; }
inline bool xy_is_8bit(const Cpu& c) { return c.e || (c.p & FX) != 0; }
inline uint16_t a_mask(const Cpu& c) {
    return a_is_8bit(c) ? 0x00FFu : 0xFFFFu;
}
inline uint16_t xy_mask(const Cpu& c) {
    return xy_is_8bit(c) ? 0x00FFu : 0xFFFFu;
}

// Banked memory: banks materialize zero-filled on first touch.
struct Mem {
    std::map<uint8_t, std::array<uint8_t, 0x10000>> banks;

    uint8_t read(uint8_t bank, uint16_t addr) {
        return banks[bank][addr];
    }
    void write(uint8_t bank, uint16_t addr, uint8_t v) {
        banks[bank][addr] = v;
    }
    uint16_t read16(uint8_t bank, uint16_t addr) {
        return uint16_t(read(bank, addr)) |
               uint16_t(read(bank, uint16_t(addr + 1))) << 8;
    }
    void load(uint8_t bank, uint16_t off,
              const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            write(bank, uint16_t(off + i), data[i]);
        }
    }
};

inline uint8_t fetch8(Cpu& c, Mem& m) {
    const uint8_t v = m.read(c.k, c.pc);
    ++c.pc;
    return v;
}

inline uint16_t fetch16(Cpu& c, Mem& m) {
    const uint8_t lo = fetch8(c, m);
    const uint8_t hi = fetch8(c, m);
    return uint16_t(lo) | uint16_t(hi) << 8;
}

inline uint32_t fetch24(Cpu& c, Mem& m) {
    const uint16_t lo = fetch16(c, m);
    const uint8_t bank = fetch8(c, m);
    return uint32_t(lo) | uint32_t(bank) << 16;
}

// Sets Z/N from `value` truncated to the accumulator's CURRENT width.
inline void set_zn_a(Cpu& c, uint16_t value) {
    const uint16_t v = value & a_mask(c);
    c.p &= uint8_t(~(FZ | FN));
    if (v == 0) c.p |= FZ;
    if (v & (a_is_8bit(c) ? 0x0080u : 0x8000u)) c.p |= FN;
}

// LDA with explicit address computation. Returns cycles consumed.
inline int lda_ea(Cpu& c, Mem& m, uint8_t bank, uint16_t addr, bool extra) {
    uint16_t v = m.read(bank, addr);
    if (!a_is_8bit(c)) {
        v |= uint16_t(m.read(bank, uint16_t(addr + 1))) << 8;
    }
    c.a = v & a_mask(c);
    set_zn_a(c, c.a);
    return extra ? 1 : 0;
}

inline int sta_ea(Cpu& c, Mem& m, uint8_t bank, uint16_t addr) {
    m.write(bank, addr, uint8_t(c.a));
    if (!a_is_8bit(c)) {
        m.write(bank, uint16_t(addr + 1), uint8_t(c.a >> 8));
    }
    return 0;
}

// Instruction lengths (needed by the disassembler): immediate loads are
// length-switchable, everything else here is fixed.
inline int insn_len(uint8_t op, bool m8, bool x8) {
    switch (op) {
        case 0xA9: return m8 ? 2 : 3;  // LDA #
        case 0xA2: return x8 ? 2 : 3;  // LDX #
        case 0xC2: case 0xE2: return 2;  // REP/SEP #imm8
        case 0xAF: case 0x8F: case 0xBF: case 0x9F:
        case 0x5C: return 4;  // long forms
        case 0xAD: case 0xBD: case 0xA5: case 0xB5:
        case 0x8D: case 0x85: case 0x4C: return 3;
        default: return 1;  // FB XCE, EA NOP, 00 BRK
    }
}

inline const char* mnemonic(uint8_t op) {
    switch (op) {
        case 0x00: return "BRK";
        case 0x85: return "STA dp";
        case 0x95: return "STA dp,X";
        case 0x8D: return "STA abs";
        case 0x8F: return "STA long";
        case 0x9F: return "STA long,X";
        case 0xA2: return "LDX #";
        case 0xA9: return "LDA #";
        case 0xAD: return "LDA abs";
        case 0xAF: return "LDA long";
        case 0xA5: return "LDA dp";
        case 0xB5: return "LDA dp,X";
        case 0xBD: return "LDA abs,X";
        case 0xBF: return "LDA long,X";
        case 0xC2: return "REP";
        case 0xE2: return "SEP";
        case 0x4C: return "JMP";
        case 0x5C: return "JML";
        case 0xEA: return "NOP";
        case 0xFB: return "XCE";
        default: return "???";
    }
}

// Executes one instruction. Returns cycles consumed, or -1 on halt
// (BRK). PC points PAST the instruction when step returns.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int step(Cpu& c, Mem& m) {
    const uint8_t op = fetch8(c, m);
    switch (op) {
        case 0x00:  // BRK: treated as deterministic halt
            return -1;
        case 0xEA:  // NOP
            return 2;
        case 0xFB: {  // XCE
            const bool old_e = c.e;
            c.e = (c.p & FC) != 0;
            if (!old_e && c.e) {
                c.p |= FM | FX;
                c.x &= 0x00FF;
                c.y &= 0x00FF;
            }
            c.p = uint8_t((c.p & ~FC) | (old_e ? FC : 0));
            return 2;
        }
        case 0xE2: {  // SEP #imm
            const uint8_t mask = fetch8(c, m);
            c.p |= mask;
            if (c.e) c.p |= FM | FX;
            return 3;
        }
        case 0xC2: {  // REP #imm
            const uint8_t mask = fetch8(c, m);
            c.p &= uint8_t(~mask);
            if (c.e) c.p |= FM | FX;
            return 3;
        }
        case 0xA9: {  // LDA #imm
            // An 8-bit immediate replaces only the low byte; the
            // hidden high byte (B) is preserved.
            if (a_is_8bit(c)) {
                c.a = uint16_t((c.a & 0xFF00u) | fetch8(c, m));
            } else {
                c.a = fetch16(c, m);
            }
            set_zn_a(c, c.a);
            return 2;
        }
        case 0xAD: {  // LDA abs
            const uint16_t addr = fetch16(c, m);
            return 4 + lda_ea(c, m, c.db, addr, false);
        }
        case 0xBD: {  // LDA abs,X
            const uint16_t base = fetch16(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const bool cross =
                (base & 0xFF00u) !=
                (uint16_t(base + idx) & 0xFF00u);
            return 4 + lda_ea(c, m, c.db, uint16_t(base + idx), cross);
        }
        case 0xA5: {  // LDA dp
            const uint8_t off = fetch8(c, m);
            const bool cross =
                ((c.d & 0x00FFu) + off) > 0x00FFu;
            return 3 + lda_ea(c, m, 0x00, uint16_t(c.d + off), cross);
        }
        case 0xB5: {  // LDA dp,X
            const uint8_t off = fetch8(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const uint16_t base = uint16_t(c.d + off);
            const bool cross = (base & 0xFF00u) !=
                               (uint16_t(base + idx) & 0xFF00u);
            return 4 + lda_ea(c, m, 0x00,
                              uint16_t(base + idx), cross);
        }
        case 0xAF: {  // LDA long
            const uint32_t addr = fetch24(c, m);
            return 5 + lda_ea(c, m, uint8_t(addr >> 16),
                              uint16_t(addr), false);
        }
        case 0xBF: {  // LDA long,X
            const uint32_t base = fetch24(c, m);
            const uint32_t addr =
                (base + (c.x & xy_mask(c))) & 0x00FFFFFFu;
            return 5 + lda_ea(c, m, uint8_t(addr >> 16),
                              uint16_t(addr), false);
        }
        case 0x8D: {  // STA abs
            const uint16_t addr = fetch16(c, m);
            sta_ea(c, m, c.db, addr);
            return 4;
        }
        case 0x85: {  // STA dp
            const uint8_t off = fetch8(c, m);
            sta_ea(c, m, 0x00, uint16_t(c.d + off));
            return 3;
        }
        case 0x95: {  // STA dp,X
            const uint8_t off = fetch8(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const uint16_t base = uint16_t(c.d + off);
            const bool cross = (base & 0xFF00u) !=
                               (uint16_t(base + idx) & 0xFF00u);
            sta_ea(c, m, 0x00, uint16_t(base + idx));
            return 4 + (cross ? 1 : 0);
        }
        case 0x8F: {  // STA long
            const uint32_t addr = fetch24(c, m);
            sta_ea(c, m, uint8_t(addr >> 16), uint16_t(addr));
            return 5;
        }
        case 0x9F: {  // STA long,X
            const uint32_t base = fetch24(c, m);
            const uint32_t addr =
                (base + (c.x & xy_mask(c))) & 0x00FFFFFFu;
            sta_ea(c, m, uint8_t(addr >> 16), uint16_t(addr));
            return 5;
        }
        case 0xA2: {  // LDX #imm (same hidden-high-byte rule as LDA)
            if (xy_is_8bit(c)) {
                c.x = uint16_t((c.x & 0xFF00u) | fetch8(c, m));
            } else {
                c.x = fetch16(c, m);
            }
            const uint16_t v = c.x & xy_mask(c);
            c.p &= uint8_t(~(FZ | FN));
            if (v == 0) c.p |= FZ;
            if (v & (xy_is_8bit(c) ? 0x0080u : 0x8000u)) c.p |= FN;
            return 2;
        }
        case 0x4C: {  // JMP abs
            c.pc = fetch16(c, m);
            return 3;
        }
        case 0x5C: {  // JML long
            const uint32_t addr = fetch24(c, m);
            c.k = uint8_t(addr >> 16);
            c.pc = uint16_t(addr);
            return 4;
        }
        default:  // uncovered opcode: deterministic halt like BRK
            return -1;
    }
}
//@LABS-STUB
inline int step(Cpu& c, Mem& m) {
    const uint8_t op = fetch8(c, m);
    switch (op) {
        case 0x00:  // BRK: treated as deterministic halt
            return -1;
        case 0xEA:  // NOP
            return 2;
        case 0xFB: {  // XCE
            const bool old_e = c.e;
            c.e = (c.p & FC) != 0;
            if (!old_e && c.e) {
                c.p |= FM | FX;
                c.x &= 0x00FF;
                c.y &= 0x00FF;
            }
            c.p = uint8_t((c.p & ~FC) | (old_e ? FC : 0));
            return 2;
        }
        case 0xE2: {  // SEP #imm
            const uint8_t mask = fetch8(c, m);
            c.p |= mask;
            if (c.e) c.p |= FM | FX;
            return 3;
        }
        case 0xC2: {  // REP #imm
            const uint8_t mask = fetch8(c, m);
            c.p &= uint8_t(~mask);
            if (c.e) c.p |= FM | FX;
            return 3;
        }
        case 0xA9: {  // LDA #imm
            // TODO(1): a seeded defect lives in THIS handler. Traces
            // diverge here as soon as the M flag is set.
            c.a = fetch16(c, m);  // BUG: always consumes two bytes and
                                  // clobbers the hidden high byte B.
            set_zn_a(c, c.a);
            return 2;
        }
        case 0xAD: {  // LDA abs
            const uint16_t addr = fetch16(c, m);
            return 4 + lda_ea(c, m, c.db, addr, false);
        }
        case 0xBD: {  // LDA abs,X
            const uint16_t base = fetch16(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const bool cross =
                (base & 0xFF00u) !=
                (uint16_t(base + idx) & 0xFF00u);
            return 4 + lda_ea(c, m, c.db, uint16_t(base + idx), cross);
        }
        case 0xA5: {  // LDA dp
            const uint8_t off = fetch8(c, m);
            const bool cross =
                ((c.d & 0x00FFu) + off) > 0x00FFu;
            return 3 + lda_ea(c, m, 0x00, uint16_t(c.d + off), cross);
        }
        case 0xB5: {  // LDA dp,X
            const uint8_t off = fetch8(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const uint16_t base = uint16_t(c.d + off);
            const bool cross = (base & 0xFF00u) !=
                               (uint16_t(base + idx) & 0xFF00u);
            return 4 + lda_ea(c, m, 0x00,
                              uint16_t(base + idx), cross);
        }
        case 0xAF: {  // LDA long
            const uint32_t addr = fetch24(c, m);
            return 5 + lda_ea(c, m, uint8_t(addr >> 16),
                              uint16_t(addr), false);
        }
        case 0xBF: {  // LDA long,X
            const uint32_t base = fetch24(c, m);
            const uint32_t addr =
                (base + (c.x & xy_mask(c))) & 0x00FFFFFFu;
            return 5 + lda_ea(c, m, uint8_t(addr >> 16),
                              uint16_t(addr), false);
        }
        case 0x8D: {  // STA abs
            const uint16_t addr = fetch16(c, m);
            sta_ea(c, m, c.db, addr);
            return 4;
        }
        case 0x85: {  // STA dp
            const uint8_t off = fetch8(c, m);
            sta_ea(c, m, 0x00, uint16_t(c.d + off));
            return 3;
        }
        case 0x95: {  // STA dp,X
            const uint8_t off = fetch8(c, m);
            const uint16_t idx = c.x & xy_mask(c);
            const uint16_t base = uint16_t(c.d + off);
            const bool cross = (base & 0xFF00u) !=
                               (uint16_t(base + idx) & 0xFF00u);
            sta_ea(c, m, 0x00, uint16_t(base + idx));
            return 4 + (cross ? 1 : 0);
        }
        case 0x8F: {  // STA long
            const uint32_t addr = fetch24(c, m);
            sta_ea(c, m, uint8_t(addr >> 16), uint16_t(addr));
            return 5;
        }
        case 0x9F: {  // STA long,X
            const uint32_t base = fetch24(c, m);
            const uint32_t addr =
                (base + (c.x & xy_mask(c))) & 0x00FFFFFFu;
            sta_ea(c, m, uint8_t(addr >> 16), uint16_t(addr));
            return 5;
        }
        case 0xA2: {  // LDX #imm (same hidden-high-byte rule as LDA)
            if (xy_is_8bit(c)) {
                c.x = uint16_t((c.x & 0xFF00u) | fetch8(c, m));
            } else {
                c.x = fetch16(c, m);
            }
            const uint16_t v = c.x & xy_mask(c);
            c.p &= uint8_t(~(FZ | FN));
            if (v == 0) c.p |= FZ;
            if (v & (xy_is_8bit(c) ? 0x0080u : 0x8000u)) c.p |= FN;
            return 2;
        }
        case 0x4C: {  // JMP abs
            c.pc = fetch16(c, m);
            return 3;
        }
        case 0x5C: {  // JML long
            const uint32_t addr = fetch24(c, m);
            c.k = uint8_t(addr >> 16);
            c.pc = uint16_t(addr);
            return 4;
        }
        default:  // uncovered opcode: deterministic halt like BRK
            return -1;
    }
}
//@LABS-END


// One canonical trace line: pc/op first, cyc last (compare_trace.py
// grammar: whitespace-separated lowercase key=value).
inline std::string trace_line(const Cpu& c, uint16_t pc_at_start,
                              uint8_t op) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "pc=%04X op=%02X k=%02X db=%02X a=%04X x=%04X y=%04X "
                  "p=%02X sp=%04X cyc=%llu",
                  unsigned(pc_at_start), unsigned(op), unsigned(c.k),
                  unsigned(c.db), c.a, c.x, c.y, c.p, c.sp,
                  static_cast<unsigned long long>(c.cycles));
    return buf;
}

const char* mnemonic(uint8_t op);

// Curriculum section 55: every CPU ships a disassembler. Renders one
// line: "<k>/<pc>: b0 b1 ..  MNEMONIC operand".
inline std::string disassemble(const Cpu& c, Mem& m, uint16_t pc) {
    char buf[160];
    const uint8_t op = m.read(c.k, pc);
    const int len = insn_len(op, a_is_8bit(c), xy_is_8bit(c));
    int n = std::snprintf(buf, sizeof(buf), "%02X/%04X:", c.k, pc);
    for (int i = 0; i < len; ++i) {
        n += std::snprintf(buf + n, sizeof(buf) - size_t(n), " %02X",
                           m.read(c.k, uint16_t(pc + i)));
    }
    std::snprintf(buf + n, sizeof(buf) - size_t(n), "  %s",
                  mnemonic(op));
    return buf;
}

}  // namespace snescpu
