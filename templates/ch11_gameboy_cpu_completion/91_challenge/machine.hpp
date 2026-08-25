#pragma once
#include <cstdint>
#include <span>
#include <cstdio>
#include <string>

#include "../01_daa_rotates/bus.hpp"
#include "../01_daa_rotates/core.hpp"
#include "../01_daa_rotates/daa_ops.hpp"
#include "../02_stack_calls/stack_ops.hpp"
#include "../03_halt_interrupts/int_ctl.hpp"

namespace gb {

// Full Chapter 11 machine: CPU with the DAA/CB and stack hooks installed,
// plus IrqHook + IntCtl over an IntBus decorator so IF ($FF0F) / IE ($FFFF)
// behave like plain registers. Shared by the smoke suite and by both the 91
// and 99 runners, so goldens stay comparable across exercises.
struct Machine {
    FlatBus bus;
    IntCtl ctl;
    IntBus ibus{bus, ctl};
    IrqHook irq{&ctl};
    Cpu cpu;

    Machine() {
        cpu.bus = &ibus;
        install_daa_hook(cpu);
        install_stack_hook(cpu);
        irq.ctl = &ctl;
        irq.install(cpu);
    }

    // Fixture images are assembled at ORG $0100 (vector-page images span
    // from $0000) and loaded at their intended base; PC reset points at
    // $0100 either way.
    void load(std::span<const uint8_t> image, uint16_t base = 0x0100) {
        bus.load(image, base);
    }

    // One instruction-boundary step with the LECTURE.md wake rules:
    // a halted CPU resumes only when (IE & IF) != 0; dispatch itself is
    // handled inside step_irq(). Returns false when trapped or asleep with
    // nothing pending (the runner's natural stop condition).
    bool step() {
        if (cpu.trap) return false;
        if (cpu.halted) {
            if (ctl.pending() == 0) return false;
            cpu.halted = false;  // woken by a pending line
        }
        step_irq(cpu, ctl);
        return true;
    }
};

// Canonical instruction trace line (curriculum §52 shape), emitted AFTER an
// instruction completes. Uppercase hex digits, lowercase keys.
inline std::string trace_line(uint16_t instr_pc, uint8_t op, const Cpu& cpu) {
    char line[160];
    std::snprintf(line, sizeof(line),
                  "pc=%04X op=%02X af=%04X bc=%04X de=%04X hl=%04X "
                  "sp=%04X cyc=%llu\n",
                  instr_pc, op, cpu.af(), cpu.bc(), cpu.de(), cpu.hl(),
                  cpu.sp, static_cast<unsigned long long>(cpu.cyc));
    return line;
}

// Canonical final-state dump: what --hash-frame writes and what the golden
// hashes are computed over.
inline std::string state_dump(const Cpu& cpu) {
    char line[128];
    std::snprintf(line, sizeof(line),
                  "af=%04X bc=%04X de=%04X hl=%04X sp=%04X pc=%04X "
                  "cyc=%llu halted=%d trap=%d\n",
                  cpu.af(), cpu.bc(), cpu.de(), cpu.hl(), cpu.sp, cpu.pc,
                  static_cast<unsigned long long>(cpu.cyc),
                  cpu.halted ? 1 : 0, cpu.trap ? 1 : 0);
    return line;
}

}  // namespace gb
