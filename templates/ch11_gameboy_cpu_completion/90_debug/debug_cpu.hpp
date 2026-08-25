#pragma once
#include <array>
#include <cstdint>

namespace gbdbg {

// Debugging drill: four excerpts of the Chapter 11 CPU machinery, each
// carrying exactly ONE seeded defect. The excerpts are self-contained on
// purpose -- they deliberately do NOT include the exercise headers -- so the
// drill cannot be solved by leaning on the real core and each defect can be
// isolated against a miniature, readable model.
//
// Each @LABS block's STUB side carries the bug (marked BUG(n)); the SOLUTION
// side shows correct behavior. No peeking until you have written down a
// hypothesis!

// Miniature CPU model: only the state the four drills touch. Flags live in
// the upper nibble of f, low nibble reads as zero (hardware behavior).
struct DbgCpu {
    enum Flag : uint8_t {
        FLAG_C = 1u << 4,
        FLAG_H = 1u << 5,
        FLAG_N = 1u << 6,
        FLAG_Z = 1u << 7,
    };

    uint8_t a{0x00}, f{0x00};
    uint8_t b{0x00}, c{0x00}, d{0x00}, e{0x00}, h{0x00}, l{0x00};
    uint16_t sp{0xFFFE}, pc{0x0100};
    bool halted{false};
    bool ime{false};
    uint64_t cyc{0};

    std::array<uint8_t, 0x10000> mem{};

    bool flag_z() const { return (f & FLAG_Z) != 0; }
    bool flag_n() const { return (f & FLAG_N) != 0; }
    bool flag_h() const { return (f & FLAG_H) != 0; }
    bool flag_c() const { return (f & FLAG_C) != 0; }
    void set_z(bool v) { f = v ? uint8_t(f | FLAG_Z) : uint8_t(f & ~FLAG_Z); }
    void set_n(bool v) { f = v ? uint8_t(f | FLAG_N) : uint8_t(f & ~FLAG_N); }
    void set_h(bool v) { f = v ? uint8_t(f | FLAG_H) : uint8_t(f & ~FLAG_H); }
    void set_c(bool v) { f = v ? uint8_t(f | FLAG_C) : uint8_t(f & ~FLAG_C); }

    uint8_t read(uint16_t address) const { return mem[address]; }
    void write(uint16_t address, uint8_t value) { mem[address] = value; }
    uint8_t fetch8() {
        const uint8_t v = mem[pc];
        pc = static_cast<uint16_t>(pc + 1);
        return v;
    }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Excerpt: DAA, addition path included. Inputs are A plus the CURRENT
// N/H/C; the half-carry adjustment must fire whenever H is set OR the low
// nibble already exceeds 9 -- either condition alone means the low nibble
// is no longer valid BCD. LECTURE.md "DAA -- the exact algorithm".
inline void daa_add_path(DbgCpu& cpu) {
    uint8_t v = cpu.a;
    if (!cpu.flag_n()) {
        if (cpu.flag_c() || v > 0x99) {
            v = static_cast<uint8_t>(v + 0x60);
            cpu.set_c(true);
        }
        if (cpu.flag_h() || (v & 0x0F) > 0x09) {
            v = static_cast<uint8_t>(v + 0x06);
        }
    } else {
        if (cpu.flag_c()) v = static_cast<uint8_t>(v - 0x60);
        if (cpu.flag_h()) v = static_cast<uint8_t>(v - 0x06);
    }
    cpu.a = v;
    cpu.set_z(v == 0);
    cpu.set_h(false);
}
//@LABS-STUB
// TODO(1): find the defect in this DAA excerpt (symptom: BCD sums with a
// half-carry come out wrong, e.g. 0x45 + 0x38 adjusting to something other
// than 0x83).
inline void daa_add_path(DbgCpu& cpu) {
    uint8_t v = cpu.a;
    if (!cpu.flag_n()) {
        if (cpu.flag_c() || v > 0x99) {
            v = static_cast<uint8_t>(v + 0x60);
            cpu.set_c(true);
        }
        if ((v & 0x0F) > 0x09) {  // BUG(1): ignores the H flag entirely
            v = static_cast<uint8_t>(v + 0x06);
        }
    } else {
        if (cpu.flag_c()) v = static_cast<uint8_t>(v - 0x60);
        if (cpu.flag_h()) v = static_cast<uint8_t>(v - 0x06);
    }
    cpu.a = v;
    cpu.set_z(v == 0);
    cpu.set_h(false);
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Excerpt: CB-page shift tail. Every CB rotate/shift finishes with Z taken
// FROM THE RESULT and N/H cleared -- unlike the base-page RLCA forms, which
// force Z=0 (LECTURE.md "Rotates vs shifts": "their CB twins set Z from the
// result").
inline void finish_cb(DbgCpu& cpu, uint8_t res) {
    cpu.set_z(res == 0);
    cpu.set_n(false);
    cpu.set_h(false);
}

// SRL r: logical right shift, C = old bit 0, bit 7 shifts in as 0.
inline uint8_t cb_srl(DbgCpu& cpu, uint8_t v) {
    const uint8_t res = static_cast<uint8_t>(v >> 1);
    cpu.set_c((v & 0x01) != 0);
    finish_cb(cpu, res);
    return res;
}
//@LABS-STUB
// TODO(2): find the defect in this CB-page flag tail (symptom: shifts that
// produce zero fail to raise Z, breaking every `shift; jr z,...` idiom).
inline void finish_cb(DbgCpu& cpu, uint8_t res) {
    cpu.set_z(false);  // BUG(2): base-page RLCA rule (Z=0 always) leaked in
    cpu.set_n(false);
    cpu.set_h(false);
}

inline uint8_t cb_srl(DbgCpu& cpu, uint8_t v) {
    const uint8_t res = static_cast<uint8_t>(v >> 1);
    cpu.set_c((v & 0x01) != 0);
    finish_cb(cpu, res);
    return res;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Excerpt: HALT handling. exec always sleeps (the sane model -- LECTURE.md
// "The HALT bug"): the driver wakes the CPU when (IE & IF) != 0 and resumes
// at the PC stored IN the CPU. Because fetching the 1-byte HALT already
// advanced PC past the instruction, that saved PC *is* the next
// instruction's address; no further skip is applied.
inline void dbg_halt(DbgCpu& cpu) { cpu.halted = true; }

// Resume address the driver jumps to when HALT wakes with IME clear
// (pending line, but no dispatch): execution continues with the
// instruction AFTER the HALT.
inline uint16_t halt_resume_pc(const DbgCpu& cpu) { return cpu.pc; }
//@LABS-STUB
// TODO(3): find the defect in the HALT resume path (symptom: after waking
// from HALT with IME clear, one extra instruction byte gets skipped --
// loops lose iterations, marker stores never execute).
inline void dbg_halt(DbgCpu& cpu) { cpu.halted = true; }

inline uint16_t halt_resume_pc(const DbgCpu& cpu) {
    // BUG(3): resumes as if HALT were a 3-byte instruction. The fetch of
    // the 1-byte opcode already advanced PC past it; adding another +2
    // swallows the following two instruction bytes.
    return static_cast<uint16_t>(cpu.pc + 2);
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Excerpt: JR cc,e with explicit conditional timing straight from the
// metadata table (LECTURE.md stack-control table: JR cc,e costs 8 T-cycles
// not taken, 12 taken -- cycles_alt = 4 pays for the internal M-cycle that
// reloads PC with the relocated address).
inline constexpr int kJrNotTakenCycles = 8;
inline constexpr int kJrTakenDelta = 4;

inline int jr_cc(DbgCpu& cpu, bool condition_met) {
    const auto offset = static_cast<int8_t>(cpu.fetch8());
    if (condition_met) {
        cpu.pc = static_cast<uint16_t>(cpu.pc + offset);
        return kJrNotTakenCycles + kJrTakenDelta;
    }
    return kJrNotTakenCycles;
}
//@LABS-STUB
// TODO(4): find the timing defect (symptom: programs heavy on taken JR
// branches run measurably fast -- every taken branch bills only the
// fall-through price).
inline constexpr int kJrNotTakenCycles = 8;
inline constexpr int kJrTakenDelta = 4;

inline int jr_cc(DbgCpu& cpu, bool condition_met) {
    const auto offset = static_cast<int8_t>(cpu.fetch8());
    if (condition_met) {
        cpu.pc = static_cast<uint16_t>(cpu.pc + offset);
        return kJrNotTakenCycles;  // BUG(4): forgets cycles += kJrTakenDelta
    }
    return kJrNotTakenCycles;
}
//@LABS-END

}  // namespace gbdbg
