#pragma once
// Line-based debugger REPL over the ch06 CHIP-8 core.
//
// Commands (curriculum ch6): step, continue, regs, memory, break, disasm,
// quit. The REPL reads from any std::istream and writes to any
// std::ostream, so scripted sessions are byte-for-byte deterministic:
// feed a script via --script and diff the output.
//
// Output contract (tests and hidden grading depend on it):
//   - prompt is "dbg> " (no newline) before every read
//   - "bye" is printed exactly once when the session ends
//   - step prints one canonical trace line per executed instruction
//   - unknown commands print: error: unknown command: <cmd>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "chip8.hpp"
#include "trace.hpp"

namespace ch06 {

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

class Debugger {
public:
    Debugger(Chip8& cpu, std::istream& in, std::ostream& out)
        : cpu_(cpu), in_(in), out_(out) {}

    // PC at or beyond this address counts as "program ended".
    void set_prog_range(uint16_t prog_end) { prog_end_ = prog_end; }

    // Reads lines until "quit" or EOF. Returns the final output only via
    // the stream; exit code semantics live in repl_main.cpp.
    void run() {
        std::string line;
        for (;;) {
            out_ << "dbg> ";
            if (!std::getline(in_, line)) break;
            if (exec_line(line)) break;
        }
        out_ << "bye\n";
    }

private:
    //@LABS-BEGIN 7
    //@LABS-SOLUTION
    bool exec_line(const std::string& line) {
        std::istringstream is(line);
        std::string cmd;
        is >> cmd;
        if (cmd.empty()) return false;
        for (char& c : cmd) c = static_cast<char>(tolower(c));
        if (cmd == "quit" || cmd == "q") return true;
        if (cmd == "help") { cmd_help(); return false; }
        if (cmd == "regs") { cmd_regs(); return false; }
        if (cmd == "memory") { cmd_memory(is); return false; }
        if (cmd == "step") { cmd_step(is); return false; }
        if (cmd == "continue") { cmd_continue(); return false; }
        if (cmd == "break") { cmd_break(is); return false; }
        if (cmd == "disasm") { cmd_disasm(is); return false; }
        out_ << "error: unknown command: " << cmd << "\n";
        return false;
    }
    //@LABS-STUB
    // TODO(7): dispatch one command line. Recognize (case-insensitive):
    // quit/q -> return true; help, regs, memory, step, continue, break,
    // disasm -> call the matching handler; anything else ->
    //   error: unknown command: <cmd>
    // Empty lines just re-prompt. Return false to keep the REPL running.
    bool exec_line(const std::string& line) {
        (void)line;
        out_ << "error: debugger not implemented yet\n";
        return false;
    }
    //@LABS-END

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    void cmd_regs() {
        for (int r = 0; r < 16; r += 4) {
            for (int j = 0; j < 4; ++j)
                out_ << "V" << "0123456789ABCDEF"[r + j]
                     << "=" << hex2(cpu_.v[r + j]) << (j == 3 ? "\n" : " ");
        }
        out_ << "I=" << hex3(cpu_.i) << " PC=" << hex4(cpu_.pc)
             << " SP=" << hex2(cpu_.sp) << " DT=" << hex2(cpu_.dt)
             << " ST=" << hex2(cpu_.st) << " CYC=" << cpu_.cycles << "\n";
    }
    //@LABS-STUB
    // TODO(1): dump registers, 4 V registers per line then the summary row:
    //   V0=00 V1=00 V2=00 V3=00
    //   ...
    //   VC=00 VD=00 VE=00 VF=00
    //   I=000 PC=0200 SP=00 DT=00 ST=00 CYC=0
    void cmd_regs() {
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void cmd_memory(std::istringstream& is) {
        unsigned addr = 0, len = 16;
        if (!parse_hex(is, addr) || !cpu_ok(addr)) {
            out_ << "error: bad arguments\n";
            return;
        }
        is >> len;
        if (is.fail()) len = 16;
        if (len == 0 || addr + len > kMemSize) {
            out_ << "error: bad arguments\n";
            return;
        }
        for (unsigned row = 0; row < len; row += 16) {
            out_ << hex4(addr + row) << ":";
            const unsigned end = (len - row < 16) ? len - row : 16;
            for (unsigned col = 0; col < end; ++col)
                out_ << " " << hex2(cpu_.mem[addr + row + col]);
            out_ << "\n";
        }
    }
    //@LABS-STUB
    // TODO(2): hex dump `memory <addr> [len]` (addr hex, len decimal,
    // default 16). One row per 16 bytes:
    //   0400: AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA AA
    // Bad/missing address or out-of-range length ->
    //   error: bad arguments
    void cmd_memory(std::istringstream& is) {
        (void)is;
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    void cmd_step(std::istringstream& is) {
        unsigned n = 1;
        is >> n;
        if (is.fail()) n = 1;
        for (unsigned i = 0; i < n && !halted(); ++i) {
            out_ << trace_line(cpu_) << "\n";
            cpu_.step();
        }
        report_halt();
    }
    //@LABS-STUB
    // TODO(3): execute `step [n]` instructions (default 1). Before each
    // execution print one canonical trace line (ch06::trace_line). Stop
    // early if the program halts.
    void cmd_step(std::istringstream& is) {
        (void)is;
    }
    //@LABS-END

    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    void cmd_continue() {
        constexpr uint64_t kStepLimit = 100000;
        uint64_t taken = 0;
        while (!halted()) {
            if (at_breakpoint()) {
                out_ << "bp hit at " << hex4(cpu_.pc) << "\n";
                return;
            }
            if (++taken > kStepLimit) {
                out_ << "halted (step limit)\n";
                return;
            }
            cpu_.step();
        }
        report_halt();
    }
    //@LABS-STUB
    // TODO(4): run until a breakpoint is REACHED (not yet executed),
    // the program ends, or 100000 steps elapse. Print exactly one of:
    //   bp hit at <pc4>
    //   halted (pc=<pc4> out of program)
    //   halted (step limit)
    void cmd_continue() {
    }
    //@LABS-END

    //@LABS-BEGIN 5
    //@LABS-SOLUTION
    void cmd_break(std::istringstream& is) {
        unsigned addr = 0;
        if (!parse_hex(is, addr) || !cpu_ok(addr)) {
            if (!breakpoints_.empty()) {
                for (size_t b = 0; b < breakpoints_.size(); ++b)
                    out_ << "breakpoint " << b << " at "
                         << hex4(breakpoints_[b]) << "\n";
                return;
            }
            out_ << "error: bad arguments\n";
            return;
        }
        breakpoints_.push_back(uint16_t(addr));
        out_ << "breakpoint " << breakpoints_.size() - 1 << " set at "
             << hex4(addr) << "\n";
    }
    //@LABS-STUB
    // TODO(5): `break <addr>` sets a breakpoint and prints
    //   breakpoint <id> set at <addr4>
    // Bare `break` lists them (`breakpoint <id> at <addr4>` per line).
    void cmd_break(std::istringstream& is) {
        (void)is;
    }
    //@LABS-END

    //@LABS-BEGIN 6
    //@LABS-SOLUTION
    void cmd_disasm(std::istringstream& is) {
        unsigned n = 5;
        is >> n;
        if (is.fail()) n = 5;
        uint16_t addr = cpu_.pc;
        for (unsigned k = 0; k < n; ++k) {
            out_ << cpu_.disassemble(addr) << "\n";
            addr = uint16_t(addr + 2);
        }
    }
    //@LABS-STUB
    // TODO(6): disassemble `disasm [n]` instructions from the current PC
    // (default 5), one Chip8::disassemble line each.
    void cmd_disasm(std::istringstream& is) {
        (void)is;
    }
    //@LABS-END

    void cmd_help() {
        out_ << "commands:\n"
             << "  step [n]            execute n instructions (default 1)\n"
             << "  continue            run until breakpoint or program end\n"
             << "  regs                dump registers\n"
             << "  memory <addr> [len] hex dump memory\n"
             << "  break [addr]        set/list breakpoints\n"
             << "  disasm [n]          disassemble n instructions from PC\n"
             << "  quit                exit the debugger\n";
    }

    static bool parse_hex(std::istringstream& is, unsigned& out) {
        std::string tok;
        if (!(is >> tok)) return false;
        if (tok.compare(0, 2, "0x") == 0 || tok.compare(0, 2, "0X") == 0)
            tok = tok.substr(2);
        if (tok.empty()) return false;
        char* end = nullptr;
        out = std::strtoul(tok.c_str(), &end, 16);
        return end && *end == '\0';
    }

    bool cpu_ok(unsigned addr) const { return addr < kMemSize; }
    bool halted() const {
        return cpu_.pc < kProgBase || cpu_.pc >= prog_end_;
    }
    bool at_breakpoint() const {
        for (uint16_t b : breakpoints_)
            if (b == cpu_.pc) return true;
        return false;
    }
    void report_halt() {
        if (halted())
            out_ << "halted (pc=" << hex4(cpu_.pc) << " out of program)\n";
    }

    Chip8& cpu_;
    std::istream& in_;
    std::ostream& out_;
    uint16_t prog_end_ = kProgBase;
    std::vector<uint16_t> breakpoints_;
};

}  // namespace ch06
