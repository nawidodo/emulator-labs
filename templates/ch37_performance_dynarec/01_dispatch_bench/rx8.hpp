#pragma once
// rx8 — toy 8-register RISC machine used throughout ch37 (performance &
// dynamic recompilation). See 01_dispatch_bench/SPEC.md for the ISA
// contract. Everything in this chapter is graded by executed-instruction /
// executed-IR-op counts, never wall time.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace rx8 {

constexpr uint32_t kMemSize = 16384;  // bytes; words live on 4-aligned addrs

enum : uint8_t {
    OP_NOP = 0x00, OP_MOV = 0x01, OP_ADD = 0x02, OP_ADDI = 0x03,
    OP_SUB = 0x04, OP_AND = 0x05, OP_OR = 0x06, OP_XOR = 0x07,
    OP_SHL = 0x08, OP_SHR = 0x09, OP_LW = 0x0A, OP_SW = 0x0B,
    OP_BEQZ = 0x0C, OP_BNEZ = 0x0D, OP_JMP = 0x0E, OP_OUT = 0x0F,
    OP_HALT = 0x10,
};

// One decoded instruction. imm12 is the RAW field: use simm() for the
// sign-extended immediate and target() for absolute branch/jump targets.
struct Decoded {
    uint8_t op = OP_NOP;
    uint8_t rd = 0, rs = 0, rt = 0;
    uint16_t imm12 = 0;

    int32_t simm() const {
        return (imm12 & 0x800) ? int32_t(imm12) - 4096 : int32_t(imm12);
    }
    uint32_t target() const { return uint32_t(imm12) << 2; }
};

inline Decoded decode(uint32_t word) {
    Decoded d;
    d.op = uint8_t(word >> 24);
    d.rd = uint8_t(word >> 20) & 0xF;
    d.rs = uint8_t(word >> 16) & 0xF;
    d.rt = uint8_t(word >> 12) & 0xF;
    d.imm12 = uint16_t(word & 0xFFF);
    return d;
}

// Assembler helper used by tests and in-source fixtures.
constexpr uint32_t enc(uint8_t op, uint8_t rd = 0, uint8_t rs = 0,
                       uint8_t rt = 0, uint16_t imm12 = 0) {
    return (uint32_t(op) << 24) | (uint32_t(rd & 0xF) << 20) |
           (uint32_t(rs & 0xF) << 16) | (uint32_t(rt & 0xF) << 12) |
           uint32_t(imm12 & 0xFFF);
}

struct Machine {
    std::array<uint32_t, 8> r{};   // r0 is hardwired zero
    uint32_t pc = 0;
    bool halted = false;
    bool fault = false;
    // Retired-instruction counter. This is the chapter's deterministic
    // performance proxy — NEVER wall time.
    uint64_t executed = 0;
    std::vector<uint32_t> out;     // output port log (observable)
    std::array<uint8_t, kMemSize> mem{};

    void reset() { *this = Machine{}; }
    void load(std::span<const uint8_t> image) {
        reset();
        auto n = std::min<size_t>(image.size(), kMemSize);
        for (size_t i = 0; i < n; ++i) mem[i] = image[i];
    }

    uint32_t load_word(uint32_t addr);
    void store_word(uint32_t addr, uint32_t value);
    int step();

    uint32_t read_le(uint32_t addr) const {
        return uint32_t(mem[addr]) | uint32_t(mem[addr + 1]) << 8 |
               uint32_t(mem[addr + 2]) << 16 | uint32_t(mem[addr + 3]) << 24;
    }
    void write_le(uint32_t addr, uint32_t v) {
        mem[addr] = uint8_t(v);
        mem[addr + 1] = uint8_t(v >> 8);
        mem[addr + 2] = uint8_t(v >> 16);
        mem[addr + 3] = uint8_t(v >> 24);
    }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint32_t Machine::load_word(uint32_t addr) {
    if (addr % 4 != 0 || addr + 4 > kMemSize) {
        fault = true;
        return 0;
    }
    return read_le(addr);
}

inline void Machine::store_word(uint32_t addr, uint32_t value) {
    if (addr % 4 != 0 || addr + 4 > kMemSize) {
        fault = true;
        return;
    }
    write_le(addr, value);
}
//@LABS-STUB
// TODO(1): enforce the memory contract from SPEC.md — word accesses must be
// 4-aligned AND fully inside memory, otherwise raise the sticky `fault`
// flag (and do nothing else). Little-endian via read_le/write_le.
inline uint32_t Machine::load_word(uint32_t addr) {
    (void)addr;
    return 0;  // wrong on purpose: no alignment/range checks yet
}

inline void Machine::store_word(uint32_t addr, uint32_t value) {
    (void)addr;
    (void)value;
    // wrong on purpose: stores nothing
}
//@LABS-END

namespace detail {
inline void set_reg(Machine& m, uint8_t rd, uint32_t v) {
    if (rd != 0) m.r[rd] = v;  // r0 is hardwired zero
}
}  // namespace detail

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Execute one already-decoded instruction against the machine state.
// The pc has ALREADY been advanced past this instruction by step(); taken
// branches/jumps OVERWRITE it. Returns true when the instruction retired.
// NOTE: SW carries its base register in rd and its stored source in rs —
// the same field positions lower_insn() preserves in exercise 03's IR.
inline bool execute(Machine& m, const Decoded& d) {
    using detail::set_reg;
    switch (d.op) {
        case OP_NOP: break;
        case OP_MOV: set_reg(m, d.rd, m.r[d.rs]); break;
        case OP_ADD: set_reg(m, d.rd, m.r[d.rs] + m.r[d.rt]); break;
        case OP_ADDI: set_reg(m, d.rd, m.r[d.rs] + uint32_t(d.simm())); break;
        case OP_SUB: set_reg(m, d.rd, m.r[d.rs] - m.r[d.rt]); break;
        case OP_AND: set_reg(m, d.rd, m.r[d.rs] & m.r[d.rt]); break;
        case OP_OR: set_reg(m, d.rd, m.r[d.rs] | m.r[d.rt]); break;
        case OP_XOR: set_reg(m, d.rd, m.r[d.rs] ^ m.r[d.rt]); break;
        case OP_SHL: set_reg(m, d.rd, m.r[d.rs] << (m.r[d.rt] & 31)); break;
        case OP_SHR: set_reg(m, d.rd, m.r[d.rs] >> (m.r[d.rt] & 31)); break;
        case OP_LW:
            set_reg(m, d.rd, m.load_word(m.r[d.rs] + uint32_t(d.simm())));
            break;
        case OP_SW:  // rd=base register, rs=stored source
            m.store_word(m.r[d.rd] + uint32_t(d.simm()), m.r[d.rs]);
            break;
        case OP_BEQZ:
            if (m.r[d.rs] == 0) m.pc = d.target();
            break;
        case OP_BNEZ:
            if (m.r[d.rs] != 0) m.pc = d.target();
            break;
        case OP_JMP: m.pc = d.target(); break;
        case OP_OUT: m.out.push_back(m.r[d.rd]); break;
        case OP_HALT: m.halted = true; break;
        default: m.fault = true; break;  // unknown opcode
    }
    return true;
}
//@LABS-STUB
// TODO(2): implement every rx8 opcode from the SPEC.md table against the
// machine state. The pc is already advanced past this instruction; taken
// branches/jumps OVERWRITE it with d.target(). Respect r0 semantics via
// detail::set_reg, append to m.out for OP_OUT, set m.halted / m.fault.
// Return true once the instruction retired. Stub retires nothing so the
// suite runs RED until you finish.
inline bool execute(Machine& m, const Decoded& d) {
    (void)m;
    (void)d;
    return false;  // wrong on purpose: dispatches nowhere
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// The classic interpreter loop: fetch, decode, execute — one switch
// dispatch per guest instruction. Returns 1 retired, 0 when the machine
// can no longer continue (halted/faulted).
inline int Machine::step() {
    if (halted || fault) return 0;
    const uint32_t at = pc;
    pc += 4;  // default advance; taken branches overwrite in execute()
    const Decoded d = decode(load_word(at));
    if (fault) return 0;
    execute(*this, d);
    ++executed;
    return 1;
}
//@LABS-STUB
// TODO(3): fetch at the current pc (decode(load_word(pc))), advance pc by
// 4 BEFORE executing so branches can overwrite it, call execute(), bump
// `executed`, and return 1. Refuse to run when halted or faulted (return
// 0). A fetch that raises fault also returns 0 without retiring.
inline int Machine::step() {
    return 0;  // wrong on purpose: never retires anything
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Run until halt, fault, or budget exhaustion. Returns the number of
// retired instructions — the benchmark score of exercise 01.
inline uint64_t run(Machine& m, uint64_t max_steps) {
    uint64_t n = 0;
    while (n < max_steps && m.step() == 1) ++n;
    return n;
}
//@LABS-STUB
// TODO(4): repeatedly step() while under the budget and the machine keeps
// retiring. Return the number of retired instructions. This count is the
// deterministic workload cost the whole chapter optimizes against.
inline uint64_t run(Machine& m, uint64_t max_steps) {
    (void)m;
    (void)max_steps;
    return 0;  // wrong on purpose: runs nothing
}
//@LABS-END

// Canonical observable-state text hashed by runners: the OUT log plus all
// nonzero aligned memory words. Register values are deliberately NOT
// observable — see SPEC.md ("Observable state") for why that is what makes
// dead-register elimination legal in ch37/04.
inline std::string observable_dump(const Machine& m) {
    char line[64];
    std::string s = "out";
    for (uint32_t v : m.out) {
        std::snprintf(line, sizeof(line), " %08x", v);
        s += line;
    }
    s += '\n';
    for (uint32_t a = 0; a < kMemSize; a += 4) {
        const uint32_t v = m.read_le(a);
        if (v != 0) {
            std::snprintf(line, sizeof(line), "mem %04x=%08x\n", a, v);
            s += line;
        }
    }
    return s;
}

// One canonical trace line (AUTHORING trace format, lowercase key=value).
inline std::string trace_line(const Machine& m, uint32_t pc_at, uint8_t op,
                              uint64_t cyc) {
    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "pc=%04x op=%02x r1=%08x r2=%08x r3=%08x r4=%08x "
                  "r5=%08x r6=%08x r7=%08x cyc=%llu",
                  pc_at, op, m.r[1], m.r[2], m.r[3], m.r[4], m.r[5], m.r[6],
                  m.r[7], static_cast<unsigned long long>(cyc));
    return buf;
}

}  // namespace rx8
