#pragma once
#include <cstdint>

#include "../02_stack_calls/stack_ops.hpp"

namespace gb {

// Chapter 11.3: interrupt sources as a pending-flag model, IME handling
// with the EI-delay quirk, and full HALT behavior. Chapter 13 wires real
// timer/LCD hardware into IF; here the flags are plain registers exposed at
// $FF0F/$FFFF through a small bus decorator.

struct IntCtl {
    uint8_t flags = 0x00;   // IF at $FF0F: bit0 VBlank .. bit4 Joypad
    uint8_t enabled = 0x00; // IE at $FFFF
    bool ime = false;
    int ei_delay = 0;       // counts instructions left until EI lands

    static constexpr uint16_t kVectors[5] = {
        0x40, 0x48, 0x50, 0x58, 0x60};  // VBlank STAT Timer Serial Joypad

    uint8_t pending() const {
        return static_cast<uint8_t>(flags & enabled);
    }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// EI takes effect only AFTER the following instruction retires; DI is
// immediate. RETI re-enables immediately (no delay) -- handled by its
// caller.
inline void exec_ei(IntCtl& ctl) { ctl.ei_delay = 2; }
inline void exec_di(IntCtl& ctl) {
    ctl.ime = false;
    ctl.ei_delay = 0;
}
//@LABS-STUB
// TODO(1): implement EI-delay (deferred enable!) and immediate DI.
inline void exec_ei(IntCtl& ctl) { (void)ctl; }  // TODO(1)
inline void exec_di(IntCtl& ctl) { (void)ctl; }  // TODO(1)
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Full HALT: always sleeps. Waking is the driver's job -- hardware resumes
// when (IE & IF) != 0, straight into the ISR when IME is set, otherwise on
// the next instruction. (Real hardware skips PC post-increment once under
// exact conditions -- the HALT bug; see LECTURE.md. We implement the sane
// version.)
inline void exec_halt(Cpu& cpu, const IntCtl& ctl) {
    (void)ctl;
    cpu.halted = true;
}
//@LABS-STUB
// TODO(2): implement HALT (set the halted flag).
inline void exec_halt(Cpu& cpu, const IntCtl& ctl) {
    (void)cpu;
    (void)ctl;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Service one interrupt if allowed. Returns cycles consumed (0 = nothing
// serviced). Order: bit 0 is the highest priority line. Clears the IF bit,
// pushes PC, jumps to the vector, drops IME. Costs 20 cycles (5 M-cycles:
// two internal, two stack writes, one set PC).
inline int service_interrupt(Cpu& cpu, IntCtl& ctl) {
    // Land the delayed EI exactly here: it applies once, after the
    // instruction boundary that follows EI.
    if (ctl.ei_delay > 0 && --ctl.ei_delay == 0) ctl.ime = true;

    const uint8_t pending = ctl.pending();
    if (!ctl.ime || pending == 0) return 0;

    for (int bit = 0; bit < 5; ++bit) {
        if (pending & (1u << bit)) {
            ctl.flags = static_cast<uint8_t>(ctl.flags & ~(1u << bit));
            ctl.ime = false;
            push16(cpu, cpu.pc);
            cpu.pc = IntCtl::kVectors[bit];
            return 20;
        }
    }
    return 0;
}

// Instruction-boundary driver used everywhere in this chapter: execute one
// instruction, retire the EI delay, then service interrupts.
inline int step_irq(Cpu& cpu, IntCtl& ctl) {
    if (cpu.trap) return 0;
    const int used = cpu.step();
    service_interrupt(cpu, ctl);
    return used;
}
//@LABS-STUB
// TODO(3): implement service_interrupt(): land the delayed EI, check
// IME and pending lines, take the highest-priority bit (clear IF, push PC,
// vector, IME=0, 20 cycles). Wire step_irq() around Cpu::step().
inline int service_interrupt(Cpu& cpu, IntCtl& ctl) {
    (void)cpu;
    (void)ctl;
    return 0;
}
inline int step_irq(Cpu& cpu, IntCtl& ctl) {
    (void)cpu;
    (void)ctl;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Hook claiming EI/DI/HALT plus RETI's IME restore. Installed together with
// an IntCtl pointer carried alongside the CPU.
struct IrqHook {
    IntCtl* ctl = nullptr;
    Cpu::Hook hook{nullptr, nullptr, nullptr};

    static bool exec(void* ctx, Cpu& cpu, uint8_t op, int& cycles) {
        IrqHook* self = static_cast<IrqHook*>(ctx);
        switch (op) {
            case 0xF3: exec_di(*self->ctl); cycles = 4; return true;
            case 0xFB: exec_ei(*self->ctl); cycles = 4; return true;
            case 0xD9:  // RETI: pop PC then raise IME immediately
                self->ctl->ime = true;
                cycles = opcode_info(op).cycles;
                return exec_ret(cpu, -1, opcode_info(op), cycles, true);
            case 0x76:
                exec_halt(cpu, *self->ctl);
                cycles = 4;
                return true;
            default:
                return false;
        }
    }

    void install(Cpu& cpu) {
        hook.exec = &IrqHook::exec;
        hook.ctx = this;
        hook.next = cpu.hooks;
        cpu.hooks = &hook;
    }
};
//@LABS-STUB
// TODO(4): build the IRQ hook covering DI/EI/HALT/RETI (RETI restores IME
// immediately, unlike EI).
struct IrqHook {
    IntCtl* ctl = nullptr;
    Cpu::Hook hook{nullptr, nullptr, nullptr};

    static bool exec(void* ctx, Cpu& cpu, uint8_t op, int& cycles) {
        // TODO(4): handle DI/EI/HALT/RETI here.
        (void)ctx; (void)cpu; (void)op; (void)cycles;
        return false;
    }

    void install(Cpu& cpu) {
        hook.exec = &IrqHook::exec;
        hook.ctx = this;
        hook.next = cpu.hooks;
        cpu.hooks = &hook;
    }
};
//@LABS-END

// Bus decorator exposing IF ($FF0F) / IE ($FFFF); every other access falls
// through to the wrapped bus. Full routing is chapter 12's business.
class IntBus : public Bus {
public:
    IntBus(Bus& inner, IntCtl& ctl) : inner_(inner), ctl_(ctl) {}

    uint8_t read(uint16_t address) override {
        if (address == 0xFF0F) return static_cast<uint8_t>(ctl_.flags | 0xE0);
        if (address == 0xFFFF) return ctl_.enabled;
        return inner_.read(address);
    }

    void write(uint16_t address, uint8_t value) override {
        if (address == 0xFF0F) ctl_.flags = value;
        else if (address == 0xFFFF) ctl_.enabled = value;
        else inner_.write(address, value);
    }

private:
    Bus& inner_;
    IntCtl& ctl_;
};

}  // namespace gb
