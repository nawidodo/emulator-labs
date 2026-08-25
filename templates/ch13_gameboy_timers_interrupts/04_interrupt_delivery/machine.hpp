#pragma once
#include <cstdint>

#include "../01_divider/bus.hpp"
#include "../01_divider/cpu.hpp"
#include "../01_divider/int_ctl.hpp"
#include "../02_tima_edge/timer_dev.hpp"
#include "../03_overflow_reload/timer_irq.hpp"

namespace gb {

// Exercise 13.04 -- the whole loop as one machine: copied CPU + IntCtl +
// TimerDevice glued through IntBus and timer_tick(). Plain composition
// (nothing left to implement); the tests pin its exact timing contract.

struct StepReport {
    int instr_cycles = 0;   // T-cycles of the instruction that ran
    bool overflow = false;  // TIMA wrapped (and reloaded) this boundary
    bool serviced = false;  // an interrupt was dispatched
    uint16_t vector = 0;    // dispatch target ($40/$48/$50/$58/$60)
    bool ime_at_dispatch = false;
};

// Bus decorator mapping $FF04-$FF07 onto the TimerDevice; every other
// access falls through to the wrapped bus (which carries IF/IE handling).
class TimerBus : public Bus {
public:
    TimerBus(Bus& inner, TimerDevice& t) : inner_(inner), t_(t) {}

    uint8_t read(uint16_t address) override {
        switch (address) {
            case 0xFF04: return t_.div.div();
            case 0xFF05: return t_.tima;
            case 0xFF06: return t_.tma;
            case 0xFF07: return t_.read_tac();
            default: return inner_.read(address);
        }
    }

    void write(uint16_t address, uint8_t value) override {
        switch (address) {
            case 0xFF04: t_.div.write_div(); return;
            case 0xFF05: t_.tima = value; return;
            case 0xFF06: t_.tma = value; return;
            case 0xFF07: t_.write_tac(value); return;
            default: inner_.write(address, value); return;
        }
    }

private:
    Bus& inner_;
    TimerDevice& t_;
};

struct TimerMachine {
    FlatBus ram;
    IntCtl ctl;
    IntBus int_bus{ram, ctl};
    TimerDevice timer;
    TimerBus bus{int_bus, timer};
    Cpu cpu;
    IrqHook hook;
    uint64_t cyc = 0;  // machine cycles: instructions AND dispatch entries

    void load(std::span<const uint8_t> program, uint16_t base = 0x0000) {
        ram.mem.fill(0);
        ram.load(program, base);  // vector-page images pass $0000
        cpu = Cpu{};
        cpu.bus = &bus;
        hook.ctl = &ctl;
        hook.install(cpu);
        cyc = 0;
    }

    // One instruction boundary. Exact ordering contract (the golden logs
    // of exercise 91 depend on it):
    //   1. execute one instruction; both cycle counters advance by `used`;
    //   2. tick the divider/timer by `used` cycles;
    //   3. service interrupts; if dispatched, advance both counters by the
    //      20 entry cycles and tick the timer through them too.
    // Callers emit one log line per reported event at the boundary's END
    // cyc value (an overflow inside step 3 is logged there as well).
    StepReport step_once() {
        StepReport rep;
        if (cpu.trap) return rep;

        rep.instr_cycles = cpu.step();
        cyc += static_cast<uint64_t>(rep.instr_cycles);
        if (timer_tick(timer, ctl, rep.instr_cycles)) rep.overflow = true;

        const int served = service_interrupt(cpu, ctl);
        if (served > 0) {
            cpu.halted = false;  // accepting an interrupt clears HALT
            rep.ime_at_dispatch = true;  // dispatch only happens under IME
            rep.serviced = true;
            rep.vector = cpu.pc;
            cpu.cyc += static_cast<uint64_t>(served);
            cyc += static_cast<uint64_t>(served);
            if (timer_tick(timer, ctl, served)) rep.overflow = true;
        }
        return rep;
    }

    // Run until the trap or the cycle budget. A HALTed CPU keeps the
    // divider running (hardware burns 4 T-cycles per stalled step). Two
    // wake paths exist: an IE & IF line seen before the boundary executes
    // one more instruction first; acceptance of the interrupt inside
    // step_once clears HALT directly.
    void run(uint64_t cycle_budget) {
        while (!cpu.trap && cyc < cycle_budget) {
            if (cpu.halted && (ctl.flags & ctl.enabled) != 0)
                cpu.halted = false;  // wake into the next boundary
            step_once();
        }
    }
};

}  // namespace gb
