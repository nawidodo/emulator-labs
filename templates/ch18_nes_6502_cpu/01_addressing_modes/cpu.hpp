// nes6502.hpp — 6502 CPU core skeleton for ch18 (flat-memory harness).
//
// Cycle accounting: every bus access (read or write) costs exactly one
// cycle, which reproduces the official cycle counts for everything this
// chapter models. Page-cross penalties and taken branches are billed as
// the extra accesses the real chip performs.
#pragma once

#include <array>
#include <cstdint>

namespace nes6502 {

enum FlagBits : uint8_t {
    FC = 0x01,  // carry
    FZ = 0x02,  // zero
    FI = 0x04,  // interrupt disable
    FD = 0x08,  // decimal (dead on the NES 2A03; still lives in P)
    FB = 0x10,  // break (only meaningful on the stacked copy of P)
    FU = 0x20,  // unused bit, always reads as 1
    FV = 0x40,  // overflow
    FN = 0x80,  // negative
};

class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t value) = 0;
};

// Flat 64 KiB RAM behind the CPU: enough for instruction-level tests.
struct FlatRam final : Bus {
    std::array<uint8_t, 0x10000> mem{};
    uint8_t read(uint16_t addr) override { return mem[addr]; }
    void write(uint16_t addr, uint8_t value) override { mem[addr] = value; }
};

struct Cpu {
    uint8_t a = 0, x = 0, y = 0;
    uint8_t sp = 0xFD;
    uint8_t p = FU | FI;
    uint16_t pc = 0;
    uint64_t cycles = 0;
    Bus* bus = nullptr;

    // ---- memory primitives: each access is one cycle -------------------
    uint8_t read(uint16_t addr) { ++cycles; return bus->read(addr); }
    void write(uint16_t addr, uint8_t v) { ++cycles; bus->write(addr, v); }
    uint8_t fetch8() { return read(pc++); }
    uint16_t fetch16() {
        const uint16_t lo = fetch8();
        return lo | uint16_t(fetch8()) << 8;
    }
    void push(uint8_t v) { write(uint16_t(0x100) | sp, v); --sp; }
    uint8_t pop() { ++sp; return read(uint16_t(0x100) | sp); }

    void set_zn(uint8_t v) {
        p = uint8_t((p & ~(FZ | FN)) | (v == 0 ? FZ : 0) | (v & FN));
    }

    void load_program(uint16_t base, const uint8_t* bytes, size_t n) {
        for (size_t i = 0; i < n; ++i) bus->write(uint16_t(base + i), bytes[i]);
        pc = base;
        cycles = 0;
    }
};

// ---------------------------------------------------------------------------
// Addressing modes.
//
// Contract: each mode consumes its own operand bytes via fetch8/fetch16,
// stores the effective address in `out`, and returns true when an indexed
// access crossed a page boundary. The step() dispatcher turns that bool into
// the +1 cycle penalty for read instructions (and Always-penalty rows bill
// it unconditionally).
// ---------------------------------------------------------------------------

using ModeFn = bool (*)(Cpu&, uint16_t&);

// Implied / accumulator modes consume no operand bytes.
inline bool mode_imp(Cpu&, uint16_t&) { return false; }
inline bool mode_acc(Cpu&, uint16_t&) { return false; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline bool mode_imm(Cpu& c, uint16_t& out) {
    out = c.pc++;
    return false;
}
//@LABS-STUB
// TODO(1): immediate — the operand byte IS the effective address.
// Advance pc past it and report no page cross.
inline bool mode_imm(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline bool mode_zp(Cpu& c, uint16_t& out) {
    out = c.fetch8();
    return false;
}

inline bool mode_zpx(Cpu& c, uint16_t& out) {
    // Zero-page indexed sums wrap in 8 bits: $80,$X=$90 addresses $0010.
    out = uint8_t(c.fetch8() + c.x);
    return false;
}

inline bool mode_zpy(Cpu& c, uint16_t& out) {
    out = uint8_t(c.fetch8() + c.y);
    return false;
}
//@LABS-STUB
// TODO(2): zero-page modes. zp reads one operand byte as the address;
// zp,X and zp,Y add the index register and MUST wrap inside page zero.
inline bool mode_zp(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(2)
}
inline bool mode_zpx(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(2)
}
inline bool mode_zpy(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(2)
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline bool mode_abs(Cpu& c, uint16_t& out) {
    out = c.fetch16();
    return false;
}

inline bool mode_absx(Cpu& c, uint16_t& out) {
    const uint16_t base = c.fetch16();
    out = uint16_t(base + c.x);
    return (base & 0xFF00) != (out & 0xFF00);
}

inline bool mode_absy(Cpu& c, uint16_t& out) {
    const uint16_t base = c.fetch16();
    out = uint16_t(base + c.y);
    return (base & 0xFF00) != (out & 0xFF00);
}
//@LABS-STUB
// TODO(3): absolute and absolute-indexed. abs fetches a little-endian
// 16-bit address; absx/absy add the index register and report whether the
// sum crossed into a new page (that drives the +1 cycle penalty).
inline bool mode_abs(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(3)
}
inline bool mode_absx(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(3)
}
inline bool mode_absy(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(3)
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline bool mode_izx(Cpu& c, uint16_t& out) {
    // (zp,X): pointer = (operand + X) & $FF; both pointer bytes stay in
    // page zero — the high-byte fetch wraps at $FF.
    const uint8_t ptr = uint8_t(c.fetch8() + c.x);
    const uint16_t lo = c.read(ptr);
    const uint16_t hi = c.read(uint8_t(ptr + 1));
    out = lo | hi << 8;
    return false;
}

inline bool mode_izy(Cpu& c, uint16_t& out) {
    // (zp),Y: read the pointer from page zero (wrapping high byte), then
    // add Y at full 16-bit width; the add can cross a page.
    const uint8_t ptr = c.fetch8();
    const uint16_t base =
        c.read(ptr) | uint16_t(c.read(uint8_t(ptr + 1))) << 8;
    out = uint16_t(base + c.y);
    return (base & 0xFF00) != (out & 0xFF00);
}

inline bool mode_ind(Cpu& c, uint16_t& out) {
    // JMP ($xxxx): pointer dereference with the famous page-wrap quirk —
    // a pointer ending in $FF reads its high byte from the SAME page.
    const uint16_t ptr = c.fetch16();
    const uint16_t lo = c.read(ptr);
    const uint16_t hi =
        c.read(uint16_t((ptr & 0xFF00) | ((ptr + 1) & 0x00FF)));
    out = lo | hi << 8;
    return false;
}
//@LABS-STUB
// TODO(4): indirect modes. izx = (zp,X), izy = (zp),Y, ind = JMP ($xxxx).
// Pointer high bytes wrap within page zero / the pointer's own page.
inline bool mode_izx(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(4)
}
inline bool mode_izy(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(4)
}
inline bool mode_ind(Cpu&, uint16_t& out) {
    (void)out;
    return false;  // TODO(4)
}
//@LABS-END

}  // namespace nes6502
