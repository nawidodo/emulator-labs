#pragma once
#include <cstdint>
#include <span>
#include "alu.hpp"
#include "bus.hpp"
#include "memops.hpp"

namespace psx::r3000a {

// User programs in this chapter load at the classic PS1 user text address.
// (The real machine boots the BIOS at 0xBFC00000; ch39 adds that layer.)
constexpr uint32_t kProgramBase = 0x80010000u;

// ---------------------------------------------------------------------------
// The delay-slot window machine.
//
// Instead of a bare "pc", the interpreter carries the triple required by the
// curriculum reference solution:
//
//   current_pc      instruction executing now
//   next_pc         what runs next unless redirected
//   in_delay_slot   current_pc is the slot of the previous taken branch
//
// One step executes at current_pc and advances the window. Because a taken
// branch only redirects next_pc, the slot instruction executes exactly once,
// after the branch, before control transfers — for jumps too, since a jump
// is just a branch whose target comes from a register.
// ---------------------------------------------------------------------------

struct Window {
    uint32_t current_pc;
    uint32_t next_pc;
    bool in_delay_slot;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Destination of an immediate branch: displacement is relative to the DELAY
// SLOT address (pc + 4), not to the branch itself.
inline uint32_t branch_target(uint32_t pc, int32_t disp16) {
    return pc + 4u + uint32_t(disp16 << 2);
}

// jal/jalr link value: the instruction AFTER the delay slot. Returning to it
// means the slot ran exactly once as part of the call.
inline uint32_t link_address(uint32_t pc) { return pc + 8u; }

// jr/jalr destination is simply the register value; no extra arithmetic.
inline uint32_t jump_target(uint32_t reg_value) { return reg_value; }
//@LABS-STUB
// TODO(1): compute branch/jump destinations.
//   branch_target(pc, disp): pc + 4 + (disp << 2)
//   link_address(pc):        pc + 8
//   jump_target(reg_value):  reg_value unchanged
inline uint32_t branch_target(uint32_t pc, int32_t disp16) {
    (void)pc;
    (void)disp16;
    return 0;  // TODO(1)
}
inline uint32_t link_address(uint32_t pc) {
    (void)pc;
    return 0;  // TODO(1)
}
inline uint32_t jump_target(uint32_t reg_value) {
    (void)reg_value;
    return 0;  // TODO(1)
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Advance the execution window by one step. `taken` means the instruction we
// just executed wants control transferred to `target` AFTER its delay slot:
// the old next_pc becomes current, the target becomes next. Jumps need no
// special case here — treating jr differently from any other taken branch is
// exactly how delay slots get skipped by accident.
inline Window advance(const Window& w, bool taken, uint32_t target) {
    Window out;
    out.current_pc = w.next_pc;
    out.next_pc = taken ? target : w.next_pc + 4u;
    out.in_delay_slot = taken;
    return out;
}
//@LABS-STUB
// TODO(2): advance the window.
//   current_pc'     = next_pc                    (slot or sequential code)
//   next_pc'        = taken ? target : next_pc+4
//   in_delay_slot'  = taken                      (a taken branch's slot follows)
inline Window advance(const Window& w, bool taken, uint32_t target) {
    (void)w;
    (void)taken;
    (void)target;
    return Window{0, 0, false};  // TODO(2)
}
//@LABS-END

enum class Flow : uint8_t { None, Taken };
struct FlowResult {
    Flow flow = Flow::None;
    uint32_t target = 0;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Branch group. blez/bgtz compare SIGNED against zero; beq/bne compare for
// equality only. REGIMM (op=0x01) selects via its rt field: 0x00 bltz,
// 0x01 bgez. The linking variants (0x10 bltzal / 0x11 bgezal) are left to
// the ch38 coding test. j/jal build their target from the delay-slot
// address's upper nibble; linking itself happens in cpu_step().
inline FlowResult exec_branch(uint32_t instr, const Regs& r, uint32_t pc) {
    const auto taken_to = [&](bool taken) {
        FlowResult f;
        if (taken) {
            f.flow = Flow::Taken;
            f.target = branch_target(pc, int32_t(int16_t(imm16(instr))));
        }
        return f;
    };
    switch (opcode(instr)) {
        case 0x04: return taken_to(r.get(rs(instr)) == r.get(rt(instr)));            // beq
        case 0x05: return taken_to(r.get(rs(instr)) != r.get(rt(instr)));            // bne
        case 0x06:                                                                   // blez
            return taken_to(int32_t(r.get(rs(instr))) <= 0);
        case 0x07:                                                                   // bgtz
            return taken_to(int32_t(r.get(rs(instr))) > 0);
        case 0x01: {                                                                 // REGIMM
            const uint32_t sel = rt(instr);
            if (sel != 0x00 && sel != 0x01) return FlowResult{};
            return taken_to(sel == 0x00 ? int32_t(r.get(rs(instr))) < 0
                                        : int32_t(r.get(rs(instr))) >= 0);
        }
        case 0x02:                                                                   // j
        case 0x03: {                                                                 // jal
            FlowResult f;
            f.flow = Flow::Taken;
            f.target = ((pc + 4u) & 0xF0000000u) | ((instr & 0x03FFFFFFu) << 2);
            return f;
        }
        default: return FlowResult{};
    }
}
//@LABS-STUB
// TODO(3): implement beq=0x04 bne=0x05 blez=0x06 bgtz=0x07, REGIMM
// bltz(rt=0x00)/bgez(rt=0x01), j=0x02, jal=0x03. Use branch_target() for
// displacements; j/jal target = ((pc+4)&0xF0000000)|(index<<2). Return
// Flow::None when not taken or unknown.
inline FlowResult exec_branch(uint32_t instr, const Regs& r, uint32_t pc) {
    (void)instr;
    (void)r;
    (void)pc;
    return FlowResult{};  // TODO(3)
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// HI/LO group. Real R3000A mult occupies ~3-10 cycles and div ~35-37 with NO
// interlock (reads of HI/LO right after issue see stale data); our model
// completes immediately but charges the documented cost. Division by zero is
// UNPREDICTABLE per the MIPS manual — we leave HI/LO untouched.
inline int exec_muldiv(uint32_t instr, Regs& r) {
    switch (funct(instr)) {
        case 0x10: r.set(rd(instr), r.hi); return 1;   // mfhi
        case 0x11: r.hi = r.get(rs(instr)); return 1;  // mthi
        case 0x12: r.set(rd(instr), r.lo); return 1;   // mflo
        case 0x13: r.lo = r.get(rs(instr)); return 1;  // mtlo
        case 0x18: {                                   // mult (signed)
            const int64_t p = int64_t(int32_t(r.get(rs(instr)))) *
                              int64_t(int32_t(r.get(rt(instr))));
            r.lo = uint32_t(p);
            r.hi = uint32_t(uint64_t(p) >> 32);
            return 5;
        }
        case 0x19: {                                   // multu
            const uint64_t p = uint64_t(r.get(rs(instr))) * uint64_t(r.get(rt(instr)));
            r.lo = uint32_t(p);
            r.hi = uint32_t(p >> 32);
            return 5;
        }
        case 0x1A: {                                   // div (signed)
            const int32_t a = int32_t(r.get(rs(instr)));
            const int32_t b = int32_t(r.get(rt(instr)));
            if (b != 0) {
                r.lo = uint32_t(a / b);
                r.hi = uint32_t(a % b);
            }
            return 37;
        }
        case 0x1B: {                                   // divu
            const uint32_t a = r.get(rs(instr));
            const uint32_t b = r.get(rt(instr));
            if (b != 0) {
                r.lo = a / b;
                r.hi = a % b;
            }
            return 37;
        }
        default: return 0;
    }
}
//@LABS-STUB
// TODO(4): implement mfhi=0x10 mthi=0x11 mflo=0x12 mtlo=0x13 mult=0x18
// multu=0x19 div=0x1A divu=0x1B; return cycle cost 1 / 5 / 37 respectively
// (documented model of real latencies). mult/div signed, multu/divu unsigned,
// div-by-zero leaves HI/LO unchanged.
inline int exec_muldiv(uint32_t instr, Regs& r) {
    (void)instr;
    (void)r;
    return 0;  // TODO(4)
}
//@LABS-END

struct StepResult {
    uint32_t pc;
    uint32_t next_pc;
    bool in_delay_slot;
    uint64_t cycles;
};

struct CpuState {
    Regs regs{};
    Window window{kProgramBase, kProgramBase + 4u, false};
    uint64_t cycles = 0;
    bool halted = false;  // syscall/break/unknown stop the run (ch39 adds exceptions)

    void load_program(Bus& bus, const uint8_t* bytes, size_t n,
                      uint32_t base = kProgramBase) {
        bus.store_bytes(base, bytes, n);
    }
};

//@LABS-BEGIN 5
//@LABS-SOLUTION
// One full instruction step: fetch at current_pc, execute, advance the
// window. syscall/break/unknown instructions halt — proper handling needs
// ch39's COP0 machinery.
inline StepResult cpu_step(CpuState& cpu, Bus& bus) {
    const uint32_t pc = cpu.window.current_pc;
    const uint32_t instr = bus.read32(pc);
    int cost = 1;
    bool taken = false;
    uint32_t target = 0;
    bool handled = true;

    switch (opcode(instr)) {
        case 0x00: {  // SPECIAL
            switch (funct(instr)) {
                case 0x21: case 0x23: case 0x24: case 0x25: case 0x26:
                case 0x27: case 0x2A: case 0x2B:
                    exec_alu_r(instr, cpu.regs);
                    break;
                case 0x00: case 0x02: case 0x03: case 0x04: case 0x06: case 0x07:
                    exec_shifts(instr, cpu.regs);
                    break;
                case 0x08:  // jr — a branch like any other; slot still executes
                    taken = true;
                    target = jump_target(cpu.regs.get(rs(instr)));
                    break;
                case 0x09: {  // jalr — read rs BEFORE overwriting it with $rd
                    const uint32_t dest = cpu.regs.get(rs(instr));
                    cpu.regs.set(rd(instr) == 0 ? 31u : rd(instr), link_address(pc));
                    taken = true;
                    target = jump_target(dest);
                    break;
                }
                case 0x0C: case 0x0D:  // syscall / break
                    cpu.halted = true;
                    break;
                case 0x10: case 0x11: case 0x12: case 0x13:
                case 0x18: case 0x19: case 0x1A: case 0x1B:
                    cost += exec_muldiv(instr, cpu.regs) - 1;
                    break;
                default:
                    handled = false;
                    break;
            }
            break;
        }
        case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
        case 0x07: {  // branch / jump family
            const FlowResult f = exec_branch(instr, cpu.regs, pc);
            if (f.flow == Flow::Taken) {
                taken = true;
                target = f.target;
                if (opcode(instr) == 0x03) cpu.regs.set(31, link_address(pc));  // jal
            }
            break;
        }
        case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: {  // loads
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            switch (opcode(instr)) {
                case 0x20: cpu.regs.set(rt(instr), do_load_byte(bus, addr, true)); break;   // lb
                case 0x21: cpu.regs.set(rt(instr), do_load_half(bus, addr, true)); break;   // lh
                case 0x23: cpu.regs.set(rt(instr), do_lw(bus, addr)); break;               // lw
                case 0x24: cpu.regs.set(rt(instr), do_load_byte(bus, addr, false)); break;  // lbu
                case 0x25: cpu.regs.set(rt(instr), do_load_half(bus, addr, false)); break;  // lhu
            }
            break;
        }
        case 0x22: case 0x26: {  // lwl / lwr keep the other half of rt
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            const uint32_t merged =
                opcode(instr) == 0x26 ? do_lwr(bus, addr, cpu.regs.get(rt(instr)))
                                      : do_lwl(bus, addr, cpu.regs.get(rt(instr)));
            cpu.regs.set(rt(instr), merged);
            break;
        }
        case 0x28: case 0x29: case 0x2B: {  // sb / sh / sw
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            switch (opcode(instr)) {
                case 0x28: do_sb(bus, addr, cpu.regs.get(rt(instr))); break;
                case 0x29: do_sh(bus, addr, cpu.regs.get(rt(instr))); break;
                case 0x2B: do_sw(bus, addr, cpu.regs.get(rt(instr))); break;
            }
            break;
        }
        case 0x2A: case 0x2E: {  // swl / swr
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            if (opcode(instr) == 0x2A) do_swl(bus, addr, cpu.regs.get(rt(instr)));
            else do_swr(bus, addr, cpu.regs.get(rt(instr)));
            break;
        }
        default:
            if (!exec_alu_i(instr, cpu.regs)) handled = false;  // addiu..lui
            break;
    }

    if (!handled) cpu.halted = true;  // deterministic stop on unknown opcodes

    if (!cpu.halted) {
        cpu.window = advance(cpu.window, taken, target);
        cpu.cycles += uint64_t(cost);
    }
    return StepResult{cpu.window.current_pc, cpu.window.next_pc,
                      cpu.window.in_delay_slot, cpu.cycles};
}
//@LABS-STUB
// TODO(5): wire everything together inside cpu_step. Fetch at
// window.current_pc, then dispatch:
//   SPECIAL: exec_alu_r set, exec_shifts set, jr (taken, target from rs),
//            jalr (read rs BEFORE linking $rd to link_address(pc)),
//            syscall/break halt, exec_muldiv range adds its cost-1.
//   Branches/jumps (op 0x01..0x07): exec_branch; jal additionally links $ra.
//   Loads 0x20 lb / 0x21 lh / 0x23 lw / 0x24 lbu / 0x25 lhu into rt;
//   unaligned pair 0x22 lwl / 0x26 lwr merging with old rt;
//   stores 0x28 sb / 0x29 sh / 0x2B sw / 0x2A swl / 0x2E swr.
//   default: exec_alu_i; false result means unknown -> halt.
// Unknown instructions set cpu.halted. When NOT halted, call advance() and
// add cost cycles. On halt, freeze state and return as-is.
inline StepResult cpu_step(CpuState& cpu, Bus& bus) {
    (void)bus;
    return StepResult{cpu.window.current_pc, cpu.window.next_pc,
                      cpu.window.in_delay_slot, cpu.cycles};  // TODO(5)
}
//@LABS-END

}  // namespace psx::r3000a
