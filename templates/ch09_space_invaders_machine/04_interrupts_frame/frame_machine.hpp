#pragma once
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "vblank_timer.hpp"

// Test-scale frame machine: a flat memory, the CPU and the vblank timers
// wired together with the SAME run loop the full machine uses (poll the
// one-shots before every step; a raised pulse goes straight into
// Cpu::interrupt). Timers are configurable so integration tests can
// observe several frames' cadence in a few thousand cycles instead of
// hundreds of thousands.

namespace si {

class FrameMachine final : public i8080::Bus {
public:
    FrameMachine() { cpu_.bus = this; }

    void configure_timing(uint64_t cycles_per_frame, uint8_t opcode_even,
                          uint8_t opcode_odd) {
        timers_.configure(cycles_per_frame, opcode_even, opcode_odd);
    }

    void load(const std::vector<uint8_t>& program) {
        for (size_t i = 0; i < program.size(); ++i) mem_[i] = program[i];
        cpu_.reset();
        cpu_.bus = this;
    }

    uint8_t read(uint16_t addr) const override { return mem_[addr]; }
    void write(uint16_t addr, uint8_t val) override { mem_[addr] = val; }

    // Run until HALT or budget. Appends canonical trace lines when `trace`
    // is non-null. Counts raises/accepts for cadence assertions.
    uint64_t run(uint64_t cycle_budget, std::ostream* trace) {
        while (!cpu_.halted && cpu_.cycles < cycle_budget) {
            const IrqRaise irq = timers_.poll(cpu_.cycles);
            if (irq.raised) {
                ++raises_;
                accepts_ += cpu_.interrupt(irq.opcode) ? 1 : 0;
                raise_cycles_.push_back(cpu_.cycles);
            }
            if (trace) *trace << trace_line();
            cpu_.step();
        }
        return cpu_.cycles;
    }

    i8080::Cpu& cpu() { return cpu_; }
    int raises() const { return raises_; }
    int accepts() const { return accepts_; }
    const std::vector<uint64_t>& raise_cycles() const { return raise_cycles_; }
    VblankTimers& timers() { return timers_; }

    std::string trace_line() const {
        char line[128];
        const uint8_t flags = uint8_t(
            (cpu_.s ? 0x80 : 0) | (cpu_.z ? 0x40 : 0) | (cpu_.ac ? 0x10 : 0) |
            (cpu_.p ? 0x04 : 0) | (cpu_.cy ? 0x01 : 0) | 0x02);
        std::snprintf(line, sizeof line,
                      "pc=%04X op=%02X af=%02X%02X bc=%02X%02X de=%02X%02X "
                      "hl=%02X%02X sp=%04X cyc=%llu\n",
                      cpu_.pc, mem_[cpu_.pc], cpu_.a, flags,
                      cpu_.b, cpu_.c, cpu_.d, cpu_.e,
                      cpu_.h, cpu_.l, cpu_.sp,
                      static_cast<unsigned long long>(cpu_.cycles));
        return std::string(line);
    }

private:
    uint8_t mem_[0x10000] = {};
    VblankTimers timers_;
    i8080::Cpu cpu_;
    int raises_ = 0;
    int accepts_ = 0;
    std::vector<uint64_t> raise_cycles_;
};

}  // namespace si
