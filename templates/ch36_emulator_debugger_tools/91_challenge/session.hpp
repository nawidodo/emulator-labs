#pragma once
// DebugSession: scripted command interpreter over CpuDebug producing a
// deterministic transcript (the fixture for the challenge gate).
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "cpudebug.hpp"
#include "fx8.hpp"

namespace challenge {

class DebugSession {
public:
    explicit DebugSession(fx8::Cpu& cpu) : dbg_(cpu), cpu_(cpu) {}

    // Execute one command line; append transcript lines to `out`.
//@LABS-BEGIN 1
//@LABS-SOLUTION
    void execute(const std::string& cmd, std::vector<std::string>& out) {
        if (cmd.rfind("step", 0) == 0) {
            const int n = cmd.size() > 4 ? std::atoi(cmd.c_str() + 5) : 1;
            for (int i = 0; i < n && !cpu_.halted; ++i) {
                const auto info = dbg_.step();
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                              "pc=%02X op=%02X a=%02X cyc=%llu",
                              info.pc_before, info.opcode, cpu_.a,
                              static_cast<unsigned long long>(cpu_.cycles));
                out.emplace_back(buf);
            }
        } else if (cmd == "regs") {
            out.push_back(dbg_.regs_json());
        } else if (cmd.rfind("disasm", 0) == 0) {
            uint8_t addr = cpu_.pc;
            if (cmd.size() > 6 && cmd != "disasm pc") {
                addr = uint8_t(std::strtoul(cmd.c_str() + 7, nullptr, 16));
            }
            out.push_back(dbg_.disasm(addr));
        } else if (cmd.rfind("bp ", 0) == 0) {
            const uint8_t addr =
                uint8_t(std::strtoul(cmd.c_str() + 3, nullptr, 16));
            dbg_.add_breakpoint(addr);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "bp=%02X set", addr);
            out.emplace_back(buf);
        } else if (cmd == "run") {
            int steps = 0;
            while (!cpu_.halted) {
                dbg_.step();
                ++steps;
                if (dbg_.check_breakpoints()) break;
            }
            char buf[48];
            std::snprintf(buf, sizeof(buf), "ran %d steps", steps);
            out.emplace_back(buf);
        } else if (cmd.rfind("mem ", 0) == 0) {
            const uint8_t addr =
                uint8_t(std::strtoul(cmd.c_str() + 4, nullptr, 16));
            char buf[32];
            std::snprintf(buf, sizeof(buf), "mem[%02X]=%02X", addr,
                          dbg_.read_mem(addr));
            out.emplace_back(buf);
        } else if (cmd == "quit") {
            out.emplace_back("bye");
        } else {
            out.push_back("?? " + cmd);
        }
    }
//@LABS-STUB
    void execute(const std::string&, std::vector<std::string>&) {
        // TODO(1): implement the command set — step/regs/disasm/bp/run/
        // mem/quit — appending one deterministic transcript line per
        // observation to `out`. Every value comes from the LIVE machine
        // through dbg_.
    }
//@LABS-END

private:
    dbg::Fx8Debug dbg_;
    fx8::Cpu& cpu_;
};

}  // namespace challenge
