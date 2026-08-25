// Debug variant of the ch36/01 debugger — carries a seeded defect.
#pragma once
// Generic CPU-debug interface (curriculum §55/§56) plus the fx8
// implementation. The interface is machine-agnostic: step / regs_json /
// disasm / read+write mem / breakpoints / watchpoints.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "fx8.hpp"

namespace dbg {

struct StepInfo {
    uint8_t pc_before = 0;
    uint8_t opcode = 0;
    int cycles = 0;
    std::vector<uint8_t> writes;  // addresses written this step (sorted)
};

class CpuDebug {
public:
    virtual ~CpuDebug() = default;
    virtual StepInfo step() = 0;
    virtual std::string regs_json() const = 0;
    virtual std::string disasm(uint8_t addr) const = 0;
    virtual uint8_t read_mem(uint8_t addr) const = 0;
    virtual void write_mem(uint8_t addr, uint8_t value) = 0;
    virtual void add_breakpoint(uint8_t addr) = 0;
    virtual void add_watchpoint(uint8_t addr) = 0;
};

class Fx8Debug : public CpuDebug {
public:
    explicit Fx8Debug(fx8::Cpu& cpu) : cpu_(cpu) {}

    // Pure function of memory at `addr` — never of live machine state.
    std::string disasm(uint8_t addr) const override {
        const uint8_t op = cpu_.mem[addr];
        auto imm = [&](int k) { return "$" + hex8(cpu_.mem[uint8_t(addr + k)]); };
        switch (op) {
            case 0x00: return "NOP";
            case 0x01: return "LDA " + imm(1);
            case 0x02: return "LDA " + imm(1);
            case 0x03: return "STA " + imm(1);
            case 0x04: return "ADD " + imm(1);
            case 0x05: return "ADD " + imm(1);
            case 0x06: return "SUB " + imm(1);
            case 0x07: return "JMP " + imm(1);
            case 0x08: return "JZ " + imm(1);
            case 0x0B: return "OUT";
            case 0xFF: return "HALT";
            default: return "?? $" + hex8(op);
        }
    }

    StepInfo step() override {
        // Snapshot memory to detect writes; simple interpreters can
        // afford this, real emulators hook the bus instead.
        uint8_t before[256];
        std::copy(cpu_.mem.begin(), cpu_.mem.end(), before);
        StepInfo info;
        info.pc_before = cpu_.pc;
        info.opcode = cpu_.mem[cpu_.pc];
        info.cycles = cpu_.step();
        for (int a = 0; a < 256; ++a) {
            if (before[a] != cpu_.mem[size_t(a)]) {
                info.writes.push_back(uint8_t(a));
                ++watch_hits_[size_t(a)];
                last_written_[size_t(a)] = cpu_.mem[size_t(a)];
            }
        }
        last_step_ = info;
        return info;
    }

    void add_breakpoint(uint8_t addr) override {
        breakpoints_.push_back({addr, 0, true});
    }
    void add_watchpoint(uint8_t addr) override {
        watchpoints_.push_back(addr);
    }

    // Called AFTER each step.
    const StepInfo* check_breakpoints() {
        for (auto& bp : breakpoints_) {
            if (bp.enabled && (cpu_.pc & 0xFE) == (bp.addr & 0xFE)) {
                ++bp.hits;
                return &last_step_;
            }
        }
        return nullptr;
    }

    std::string regs_json() const override {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "{\"a\":%u,\"x\":%u,\"y\":%u,\"pc\":%u,"
                      "\"z\":%s,\"c\":%s,\"cycles\":%llu}",
                      cpu_.a, cpu_.x, cpu_.y, cpu_.pc,
                      cpu_.z ? "true" : "false", cpu_.c ? "true" : "false",
                      static_cast<unsigned long long>(cpu_.cycles));
        return buf;
    }

    uint8_t read_mem(uint8_t addr) const override { return cpu_.mem[addr]; }
    void write_mem(uint8_t addr, uint8_t value) override {
        cpu_.mem[addr] = value;
    }

    int breakpoint_hits(uint8_t addr) const {
        for (const auto& bp : breakpoints_)
            if (bp.addr == addr) return bp.hits;
        return -1;
    }
    int watch_hits(uint8_t addr) const { return watch_hits_[addr]; }
    uint8_t last_written(uint8_t addr) const { return last_written_[addr]; }
    bool has_watchpoint(uint8_t addr) const {
        for (uint8_t w : watchpoints_)
            if (w == addr) return true;
        return false;
    }

private:
    static std::string hex8(unsigned v) {
        const char* d = "0123456789ABCDEF";
        std::string s;
        s += d[(v >> 4) & 0xF];
        s += d[v & 0xF];
        return s;
    }

    struct Bp {
        uint8_t addr;
        int hits;
        bool enabled;
    };
    fx8::Cpu& cpu_;
    std::vector<Bp> breakpoints_;
    std::vector<uint8_t> watchpoints_;
    int watch_hits_[256] = {};
    uint8_t last_written_[256] = {};
    StepInfo last_step_{};
};

}  // namespace dbg
