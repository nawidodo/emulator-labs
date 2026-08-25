#pragma once
#include <cstdint>

#include "../01_daa_rotates/core.hpp"

namespace gb {

// Chapter 11.2: stack operations -- PUSH/POP/CALL/RET/RET cc/RST and the
// SP-relative arithmetic family. Conditional timing is explicit everywhere:
// not-taken cost in `cycles`, taken delta added on the spot.

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void push16(Cpu& cpu, uint16_t v) {
    cpu.bus->write(static_cast<uint16_t>(cpu.sp - 1),
                   static_cast<uint8_t>(v >> 8));
    cpu.bus->write(static_cast<uint16_t>(cpu.sp - 2),
                   static_cast<uint8_t>(v));
    cpu.sp = static_cast<uint16_t>(cpu.sp - 2);
}

inline uint16_t pop16(Cpu& cpu) {
    const uint8_t lo = cpu.bus->read(cpu.sp);
    const uint8_t hi = cpu.bus->read(static_cast<uint16_t>(cpu.sp + 1));
    cpu.sp = static_cast<uint16_t>(cpu.sp + 2);
    return static_cast<uint16_t>(lo | hi << 8);
}

// POP AF must re-apply the read-as-zero rule for F's low nibble.
inline void pop_af(Cpu& cpu) {
    const uint16_t af = pop16(cpu);
    cpu.a = static_cast<uint8_t>(af >> 8);
    cpu.f = static_cast<uint8_t>(af & 0xF0);
}
//@LABS-STUB
// TODO(1): implement PUSH/POP and POP AF (F low nibble masked!).
inline void push16(Cpu& cpu, uint16_t v) { (void)cpu; (void)v; }
inline uint16_t pop16(Cpu& cpu) {
    (void)cpu;
    return 0;
}
inline void pop_af(Cpu& cpu) { (void)cpu; }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// CALL nn / CALL cc,nn. Not-taken: 12 cycles. Taken: 12 + cycles_alt(12)
// because the push adds two internal M-cycles after the memory writes.
inline bool exec_call(Cpu& cpu, int cond_idx, const Instruction& info,
                      int& cycles) {
    const uint16_t target = cpu.fetch16();
    if (cond_idx < 0 || cpu.condition(cond_idx)) {
        const uint16_t ret_pc = cpu.pc;
        push16(cpu, ret_pc);
        cpu.pc = target;
        cycles += info.cycles_alt;
        return true;
    }
    return false;
}

// RET / RET cc,nn / RETI. Not-taken RET cc: 8 cycles; taken: +12.
inline bool exec_ret(Cpu& cpu, int cond_idx, const Instruction& info,
                     int& cycles, bool /*enable_ime*/) {
    if (cond_idx >= 0 && !cpu.condition(cond_idx)) return false;
    cpu.pc = pop16(cpu);
    cycles += info.cycles_alt;  // unconditional rows carry alt=0
    return true;
}

// RST $NN: 1-byte CALL to a fixed vector.
inline void exec_rst(Cpu& cpu, uint8_t op) {
    const uint16_t vector = static_cast<uint16_t>((op & 0x38));
    push16(cpu, cpu.pc);
    cpu.pc = vector;
}
//@LABS-STUB
// TODO(2): implement CALL/RET (with conditional cycle deltas from the
// metadata table) and RST vectors.
inline bool exec_call(Cpu& cpu, int cond_idx, const Instruction& info,
                      int& cycles) {
    (void)cpu; (void)cond_idx; (void)info; (void)cycles;
    return false;
}
inline bool exec_ret(Cpu& cpu, int cond_idx, const Instruction& info,
                     int& cycles, bool enable_ime) {
    (void)cpu; (void)cond_idx; (void)info; (void)cycles; (void)enable_ime;
    return false;
}
inline void exec_rst(Cpu& cpu, uint8_t op) { (void)cpu; (void)op; }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// SP+offset shared by E8/F8. Hardware computes H/C on the RAW unsigned byte
// even when the displacement is negative; Z/N always clear.
inline uint16_t sp_plus_e(Cpu& cpu, uint8_t raw_e) {
    const auto offset = static_cast<int8_t>(raw_e);
    const uint32_t sum =
        static_cast<uint32_t>(cpu.sp) + static_cast<uint32_t>(offset);
    cpu.set_z(false);
    cpu.set_n(false);
    cpu.set_h(((cpu.sp & 0xF) + (raw_e & 0xF)) > 0xF);
    cpu.set_c(((cpu.sp & 0xFF) + raw_e) > 0xFF);
    return static_cast<uint16_t>(sum);
}

inline int add_sp_e(Cpu& cpu) {  // E8
    const uint8_t raw = cpu.fetch8();
    cpu.sp = sp_plus_e(cpu, raw);
    return 16;
}

inline int ld_hl_sp_e(Cpu& cpu) {  // F8
    const uint8_t raw = cpu.fetch8();
    cpu.set_hl(sp_plus_e(cpu, raw));
    return 12;
}
//@LABS-STUB
// TODO(3): implement ADD SP,e [E8, 16 cyc] and LD HL,SP+e [F8, 12 cyc]
// with the raw-byte carry contract above.
inline uint16_t sp_plus_e(Cpu& cpu, uint8_t raw_e) {
    (void)cpu; (void)raw_e;
    return 0;
}
inline int add_sp_e(Cpu& cpu) {
    (void)cpu;
    return 0;
}
inline int ld_hl_sp_e(Cpu& cpu) {
    (void)cpu;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Hook for the whole stack page. `ime` handling for RETI lives in chapter
// 11.3; here RETI behaves like RET (the hook signature keeps a slot open).
inline bool stack_exec(Cpu& cpu, uint8_t op, int& cycles) {
    const int x = op >> 6;
    if (x != 3) return false;
    const int y = (op >> 3) & 7;

    // C0/C4/C8/CC pattern: even y -> RET cc, odd -> CALL cc
    if (op == 0xC9) {  // RET
        const gb::Instruction& info = opcode_info(op);
        cycles = info.cycles;
        return exec_ret(cpu, -1, info, cycles, false);
    }
    if (op == 0xD9) {  // RETI
        const gb::Instruction& info = opcode_info(op);
        cycles = info.cycles;
        return exec_ret(cpu, -1, info, cycles, true);
    }
    if ((op & 0x0F) == 0x00 || (op & 0x0F) == 0x08) {
        if (y <= 7 && (op & 0xE7) == 0xC0) {  // RET cc
            const gb::Instruction& info = opcode_info(op);
            cycles = info.cycles;
            exec_ret(cpu, y & 3, info, cycles, false);
            return true;
        }
    }
    switch (op) {
        case 0xC2: case 0xCA: case 0xD2: case 0xDA:  // JP cc handled by core
            return false;
        case 0xC4: case 0xCC: case 0xD4: case 0xDC: {  // CALL cc
            const gb::Instruction& info = opcode_info(op);
            cycles = info.cycles;
            exec_call(cpu, (op >> 3) & 3, info, cycles);
            return true;
        }
        case 0xCD: {  // CALL nn
            const gb::Instruction& info = opcode_info(op);
            cycles = info.cycles;
            exec_call(cpu, -1, info, cycles);
            return true;
        }
        case 0xC5: push16(cpu, cpu.bc());
                   cycles = opcode_info(op).cycles; return true;
        case 0xD5: push16(cpu, cpu.de());
                   cycles = opcode_info(op).cycles; return true;
        case 0xE5: push16(cpu, cpu.hl());
                   cycles = opcode_info(op).cycles; return true;
        case 0xF5: push16(cpu, cpu.af());  // af() already masks F
                   cycles = opcode_info(op).cycles; return true;
        case 0xE8: cycles = add_sp_e(cpu); return true;
        case 0xF8: cycles = ld_hl_sp_e(cpu); return true;
        default: break;
    }
    if ((op & 0xC7) == 0xC7) {  // RST row
        cycles = opcode_info(op).cycles;
        exec_rst(cpu, op);
        return true;
    }
    return false;
}
//@LABS-STUB
// TODO(4): wire the stack hook: RET/RETI/RET cc, CALL nn/cc, PUSH/POP
// (POP AF masks F!), E8/F8, RST rows. Everything else returns false.
inline bool stack_exec(Cpu& cpu, uint8_t op, int& cycles) {
    (void)cpu; (void)op; (void)cycles;
    return false;
}
//@LABS-END

namespace detail {
inline bool stack_hook_fn(void*, Cpu& cpu, uint8_t op, int& cycles) {
    return stack_exec(cpu, op, cycles);
}
}  // namespace detail

inline void install_stack_hook(Cpu& cpu) {
    static Cpu::Hook hook{&detail::stack_hook_fn, nullptr, nullptr};
    hook.next = cpu.hooks;
    cpu.hooks = &hook;
}

}  // namespace gb
