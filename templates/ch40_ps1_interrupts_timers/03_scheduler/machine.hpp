#pragma once
//
// ch40 / 03_scheduler — deterministic machine integration
//
// A tiny but complete PS1 slice: a bus with RAM + the interrupt/timer MMIO
// range, a MIPS-I subset CPU stub (LUI/ORI/ADDIU/LW/SW/BEQ/BNE, real branch
// delay slots), and video timing signals — all sequenced by the event
// scheduler from scheduler.hpp. No wall time anywhere: one call to
// run_until() replays the exact same instruction/tick interleaving forever.
//
// Synthetic GPU timing used for hblank/vblank/dotclock signals:
//
//   sysclk cycle:  0 ......... 199 | 200 ......... 399 | ...
//   hblank:        [---- 40 cyc ---]                  period 200
//   vblank:        every 25 hblanks (5000 cycles), 400 cycles wide
//   dotclock:      one pulse every 6 sysclk cycles
//   instruction:   retires every 2 system cycles

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "irq.hpp"
#include "scheduler.hpp"
#include "timers.hpp"

namespace ps1::sysdev {

constexpr uint64_t kHblankPeriod = 200;
constexpr uint64_t kHblankWidth = 40;
constexpr uint64_t kVblankPeriod = kHblankPeriod * 25;
constexpr uint64_t kVblankWidth = 400;
constexpr uint64_t kCyclesPerInstruction = 2;
constexpr uint32_t kRamSize = 1u << 21;        // 2 MiB main RAM
constexpr uint32_t kProgramLoad = 0x80010000;  // entry point for fixtures

struct VideoSignals {
    static bool hblank_level(uint64_t c) { return c % kHblankPeriod < kHblankWidth; }
    static bool vblank_level(uint64_t c) { return c % kVblankPeriod < kVblankWidth; }
    static bool hblank_pulse(uint64_t c) { return c % kHblankPeriod == 0; }
    static bool dot_pulse(uint64_t c) { return c % 6 == 0; }
};

// ---- Mini MIPS-I subset CPU -------------------------------------------------

struct MiniCpu {
    uint32_t regs[32] = {};
    uint32_t pc = kProgramLoad;
    uint32_t next_pc = kProgramLoad + 4;

    void reset(uint32_t entry = kProgramLoad) {
        pc = entry;
        next_pc = entry + 4;
    }
};

class Machine {
public:
    IrqController irq;
    TimerBank timers;
    Scheduler sched;
    MiniCpu cpu;
    std::vector<uint8_t> ram = std::vector<uint8_t>(kRamSize, 0u);
    uint64_t cycles = 0;
    std::string* trace_sink = nullptr;   // when set, one line per instruction

    void load_program(const uint32_t* words, size_t n,
                      uint32_t phys = kProgramLoad & (kRamSize - 1)) {
        for (size_t i = 0; i < n; ++i) {
            const uint32_t a = phys + static_cast<uint32_t>(i * 4);
            ram[a] = words[i] & 0xFFu;
            ram[a + 1] = (words[i] >> 8) & 0xFFu;
            ram[a + 2] = (words[i] >> 16) & 0xFFu;
            ram[a + 3] = (words[i] >> 24) & 0xFFu;
        }
        cpu.reset(kProgramLoad);
        cycles = 0;
    }

    // ---- Bus ----------------------------------------------------------------

    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    uint32_t read32(uint32_t addr) {
        addr &= 0x1FFFFFFFu;               // collapse kuseg/kseg0/kseg1
        if (addr >= 0x1F801000u && addr < 0x1F802000u) return mmio_read(addr);
        if (addr + 3 < kRamSize) {
            return uint32_t{ram[addr]} | (uint32_t{ram[addr + 1]} << 8) |
                   (uint32_t{ram[addr + 2]} << 16) |
                   (uint32_t{ram[addr + 3]} << 24);
        }
        return 0xFFFFFFFFu;                // unmapped open bus
    }

    void write32(uint32_t addr, uint32_t value) {
        addr &= 0x1FFFFFFFu;
        if (addr >= 0x1F801000u && addr < 0x1F802000u) {
            mmio_write(addr, value);
            return;
        }
        if (addr + 3 < kRamSize) {
            ram[addr] = value & 0xFFu;
            ram[addr + 1] = (value >> 8) & 0xFFu;
            ram[addr + 2] = (value >> 16) & 0xFFu;
            ram[addr + 3] = (value >> 24) & 0xFFu;
        }
    }
    //@LABS-STUB
    uint32_t read32(uint32_t /*addr*/) {
        // TODO(4): decode RAM vs the 1F801xxxh MMIO window and return data.
        return 0;
    }
    void write32(uint32_t /*addr*/, uint32_t /*value*/) {
        // TODO(4): route writes into RAM or the timer/interrupt registers.
    }
    //@LABS-END

    // ---- Execution ------------------------------------------------------------

    // Execute ONE instruction (2 cycles). Returns {pc, op} for tracing.
    struct Step { uint32_t pc; uint32_t op; };

    //@LABS-BEGIN 5
    //@LABS-SOLUTION
    Step step_cpu() {
        const uint32_t this_pc = cpu.pc;
        const uint32_t op = read32(this_pc);
        cpu.pc = cpu.next_pc;
        cpu.next_pc = cpu.pc + 4;
        const uint32_t opcd = op >> 26;
        const unsigned rs = (op >> 21) & 31, rt = (op >> 16) & 31;
        const uint16_t imm = op & 0xFFFFu;
        const uint32_t sx = imm - 0x10000u * ((imm >> 15) & 1u);
        switch (opcd) {
            case 0x00:                     // SLL $0 = NOP (and friends)
                break;
            case 0x04:                     // BEQ
                if (cpu.regs[rs] == cpu.regs[rt])
                    cpu.next_pc = this_pc + 4 + (sx << 2);
                break;
            case 0x05:                     // BNE
                if (cpu.regs[rs] != cpu.regs[rt])
                    cpu.next_pc = this_pc + 4 + (sx << 2);
                break;
            case 0x09:                     // ADDIU
                if (rt) cpu.regs[rt] = cpu.regs[rs] + sx;
                break;
            case 0x0D:                     // ORI
                if (rt) cpu.regs[rt] = cpu.regs[rs] | imm;
                break;
            case 0x0F:                     // LUI
                if (rt) cpu.regs[rt] = uint32_t{imm} << 16;
                break;
            case 0x23:                     // LW
                if (rt) cpu.regs[rt] = read32(cpu.regs[rs] + sx);
                break;
            case 0x2B:                     // SW
                write32(cpu.regs[rs] + sx, cpu.regs[rt]);
                break;
            default:
                break;                     // unknown: deterministic NOP
        }
        cpu.regs[0] = 0;                   // $zero is hardwired
        return {this_pc, op};
    }
    //@LABS-STUB
    Step step_cpu() {
        // TODO(5): fetch, decode and execute the MIPS-I subset with proper
        // branch delay slots, advancing both pc and next_pc.
        cpu.pc += 4;
        cpu.next_pc += 4;
        return {0u, 0u};
    }
    //@LABS-END

    // ---- System tick (invoked by the scheduler once per cycle) --------------

    static constexpr int kTickId = 1;

    //@LABS-BEGIN 6
    //@LABS-SOLUTION
    static void tick_cb(void* self) {
        auto* m = static_cast<Machine*>(self);
        const uint64_t c = m->sched.now();
        TimerSignals s;
        s.dot_pulse = VideoSignals::dot_pulse(c);
        s.hblank_pulse = VideoSignals::hblank_pulse(c);
        s.hblank_level = VideoSignals::hblank_level(c);
        s.vblank_level = VideoSignals::vblank_level(c);
        m->timers.tick(s, &on_timer_irq, m);
        m->sched.schedule(c + 1, kTickId, &tick_cb, m);   // periodic re-arm
    }

    static void on_timer_irq(void* self, int timer, bool asserted) {
        static constexpr uint32_t kTimerLine[3] = {kIrqTimer0, kIrqTimer1,
                                                   kIrqTimer2};
        auto* m = static_cast<Machine*>(self);
        // Timers are pulse sources: asserting latches the request into
        // I_STAT, deasserting drops the raw line so a later ack sticks.
        if (asserted)
            m->irq.raise(kTimerLine[timer]);
        else
            m->irq.lower(kTimerLine[timer]);
    }
    //@LABS-STUB
    static void tick_cb(void* /*self*/) {
        // TODO(6): sample the video signals of the current cycle, advance
        // the root counters, deliver rising edges to I_STAT and reschedule.
    }
    static void on_timer_irq(void* /*self*/, int /*timer*/, bool /*a*/) {}
    //@LABS-END

    void ensure_started() {
        if (!started_) {
            started_ = true;
            sched.schedule(cycles + 1, kTickId, &tick_cb, this);
        }
    }

    // Run whole instructions until `limit` cycles have elapsed. Each
    // instruction consumes exactly two system ticks delivered by the
    // scheduler before it retires.
    void run_until(uint64_t limit) {
        ensure_started();
        while (cycles + kCyclesPerInstruction <= limit) {
            sched.run_to(cycles + kCyclesPerInstruction);
            cycles += kCyclesPerInstruction;
            const Step st = step_cpu();
            if (trace_sink)
                *trace_sink +=
                    format_trace(st.pc, st.op, irq.status(), cycles);
        }
    }

    //@LABS-BEGIN 7
    //@LABS-SOLUTION
    static std::string format_trace(uint32_t pc, uint32_t op, uint32_t irq,
                                    uint64_t cyc) {
        // The trace shows the RAW latched I_STAT lines, independent of
        // I_MASK: poll loops read status before touching the mask.
        char line[64];
        std::snprintf(line, sizeof(line), "pc=%08X op=%08X irq=%04X cyc=%llu\n",
                      pc, op, irq & 0xFFFFu,
                      static_cast<unsigned long long>(cyc));
        return line;
    }
    //@LABS-STUB
    static std::string format_trace(uint32_t /*pc*/, uint32_t /*op*/,
                                    uint32_t /*irq*/, uint64_t /*cyc*/) {
        // TODO(7): render one trace line in the canonical key=value shape:
        // pc=<hex> op=<hex> irq=<hex> cyc=<n>
        return "";
    }
    //@LABS-END

private:
    bool started_ = false;

    uint32_t mmio_read(uint32_t addr) {
        if (addr == 0x1F801070u) return irq.status() & 0xFFFFu;
        if (addr == 0x1F801074u) return irq.read_mask();
        for (int n = 0; n < kTimerCount; ++n) {
            const uint32_t base = 0x1F801100u + 0x10u * n;
            switch (addr - base) {
                case 0x0: return timers.read_counter(n);
                case 0x4: return timers.read_mode(n);
                case 0x8: return timers.regs[n].target;
                default: break;
            }
        }
        return 0xFFFFFFFFu;                // unmapped MMIO slot
    }

    void mmio_write(uint32_t addr, uint32_t value) {
        if (addr == 0x1F801070u) {
            irq.ack(value);                // I_STAT write = acknowledge
            return;
        }
        if (addr == 0x1F801074u) {
            irq.write_mask(value);
            return;
        }
        for (int n = 0; n < kTimerCount; ++n) {
            const uint32_t base = 0x1F801100u + 0x10u * n;
            switch (addr - base) {
                case 0x0: timers.write_counter(n, value & 0xFFFFu); return;
                case 0x4: timers.write_mode(n, value & 0xFFFFu); return;
                case 0x8: timers.write_target(n, value & 0xFFFFu); return;
                default: break;
            }
        }
    }
};

}  // namespace ps1::sysdev
