#pragma once
// Trace writer — the "trace-first debugging" workhorse (curriculum §54).
//
// Canonical line format (whitespace-separated key=value, lowercase keys),
// consumed by tools/labs/compare_trace.py:
//
//   pc=0200 op=00E0 V0=00 I=000 SP=0F cyc=11
//
// Full dump mode (--trace-full) additionally carries every V register and
// both timers, so quirk-level divergences in ANY register are visible:
//
//   pc=0200 op=A21E V0=00 V1=00 ... VF=01 I=021E SP=00 DT=03 ST=00 cyc=7

#include <cstdio>
#include <ostream>
#include <sstream>
#include <string>

#include "chip8.hpp"

inline std::string hex2(uint32_t v) {
    char buf[8];
    snprintf(buf, sizeof buf, "%02X", v);
    return buf;
}
inline std::string hex3(uint32_t v) {
    char buf[8];
    snprintf(buf, sizeof buf, "%03X", v & 0xFFF);
    return buf;
}
inline std::string hex4(uint32_t v) {
    char buf[8];
    snprintf(buf, sizeof buf, "%04X", v);
    return buf;
}

namespace ch06 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint16_t peek_op(const Chip8& c) {
    return uint16_t(c.mem[c.pc] << 8 | c.mem[(c.pc + 1) & 0xFFF]);
}

inline std::string trace_line(const Chip8& c) {
    std::ostringstream os;
    os << "pc=" << hex4(c.pc) << " op=" << hex4(peek_op(c))
       << " V0=" << hex2(c.v[0]) << " I=" << hex3(c.i)
       << " SP=" << hex2(c.sp) << " cyc=" << c.cycles;
    return os.str();
}
//@LABS-STUB
// TODO(1): emit one canonical trace line for the given CPU state.
// Format (exact): pc=%03X op=%04X V0=%02X I=%03X SP=%02X cyc=%decimal
inline std::string trace_line(const Chip8& c) {
    (void)c;
    return "pc=0000 op=0000 V0=00 I=000 SP=00 cyc=0";  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline std::string full_trace_line(const Chip8& c) {
    std::ostringstream os;
    os << "pc=" << hex4(c.pc) << " op=" << hex4(peek_op(c));
    for (int r = 0; r < 16; ++r)
        os << " V" << "0123456789ABCDEF"[r] << "=" << hex2(c.v[r]);
    os << " I=" << hex3(c.i) << " SP=" << hex2(c.sp)
       << " DT=" << hex2(c.dt) << " ST=" << hex2(c.st)
       << " cyc=" << c.cycles;
    return os.str();
}
//@LABS-STUB
// TODO(2): full-dump variant: pc, op, ALL of V0..VF (%02X each), then
// I=%03X SP=%02X DT=%02X ST=%02X cyc=%decimal.
inline std::string full_trace_line(const Chip8& c) {
    (void)c;
    return "";  // wrong on purpose
}
//@LABS-END

// Streams one line per instruction. Call log() BEFORE cpu.step(): the line
// describes the instruction about to execute (matches compare_trace.py's
// expectation that golden and actual traces align per instruction).
class TraceWriter {
public:
    TraceWriter(std::ostream& out, bool full = false)
        : out_(out), full_(full) {}

    void log(const Chip8& cpu) { out_ << select(cpu) << "\n"; }

private:
    std::string select(const Chip8& cpu) const {
        return full_ ? full_trace_line(cpu) : trace_line(cpu);
    }
    std::ostream& out_;
    bool full_;
};

}  // namespace ch06
