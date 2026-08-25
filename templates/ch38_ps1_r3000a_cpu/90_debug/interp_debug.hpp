#pragma once
#include <cstdint>
#include <span>
#include "alu.hpp"
#include "bus.hpp"
#include "memops.hpp"

namespace psx::r3000a {

constexpr uint32_t kProgramBase = 0x80010000u;

// Debug exercise: this interpreter is IDENTICAL to the ch38_03 reference
// except for five seeded defects concentrated in the delay-slot machinery
// below. Everything outside the @LABS blocks is known-good.

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
//@LABS-STUB
// BUG?(): branch destination arithmetic is subtly wrong here. Compare with
// the MIPS rule: displacement counts from the delay-slot address.
// TODO(1): find and fix this seeded defect.
inline uint32_t branch_target(uint32_t pc, int32_t disp16) {
    return pc + uint32_t(disp16 << 2);
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// jal/jalr link value: the instruction AFTER the delay slot, so returning
// never re-executes the slot.
inline uint32_t link_address(uint32_t pc) { return pc + 8u; }
//@LABS-STUB
// BUG?(): calls return to the wrong address — symptoms show up on RETURN,
// not on call. (One instruction too early.)
// TODO(2): find and fix this seeded defect.
inline uint32_t link_address(uint32_t pc) { return pc + 4u; }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// jr/jalr destination is simply the register value; no compensation of any
// kind — the window machine already executes the slot before transferring.
inline uint32_t jump_target(uint32_t reg_value) { return reg_value; }
//@LABS-STUB
// BUG?(): someone "fixed" a perceived skipped-instruction problem here.
// TODO(3): find and fix this seeded defect.
inline uint32_t jump_target(uint32_t reg_value) { return reg_value + 4u; }
//@LABS-END

inline bool in_delay_slot_after(bool taken);  // declared here, seeded below
//@LABS-BEGIN 4
//@LABS-SOLUTION
// Advance the execution window. Taken branches redirect next_pc only; the
// old next_pc (the slot, or sequential code) becomes current.
inline Window advance(const Window& w, bool taken, uint32_t target) {
    Window out;
    out.current_pc = w.next_pc;
    out.next_pc = taken ? target : w.next_pc + 4u;
    out.in_delay_slot = in_delay_slot_after(taken);
    return out;
}
//@LABS-STUB
// BUG?(): the polarity of the redirect is inverted somewhere in this update.
// TODO(4): find and fix this seeded defect.
inline Window advance(const Window& w, bool taken, uint32_t target) {
    Window out;
    out.current_pc = w.next_pc;
    out.next_pc = taken ? w.next_pc + 4u : target;
    out.in_delay_slot = in_delay_slot_after(taken);
    return out;
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
inline bool in_delay_slot_after(bool taken) { return taken; }
//@LABS-STUB
// BUG?(): the tracer claims nothing ever runs in a delay slot.
inline bool in_delay_slot_after(bool) {
    return false;  // BUG?() — TODO(5): find and fix this seeded defect.
}
//@LABS-END

enum class Flow : uint8_t { None, Taken };
struct FlowResult {
    Flow flow = Flow::None;
    uint32_t target = 0;
};

// ---- known-good execution logic (identical to ch38_03) --------------------
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
        case 0x04: return taken_to(r.get(rs(instr)) == r.get(rt(instr)));
        case 0x05: return taken_to(r.get(rs(instr)) != r.get(rt(instr)));
        case 0x06: return taken_to(int32_t(r.get(rs(instr))) <= 0);
        case 0x07: return taken_to(int32_t(r.get(rs(instr))) > 0);
        case 0x01: {
            const uint32_t sel = rt(instr);
            if (sel != 0x00 && sel != 0x01) return FlowResult{};
            return taken_to(sel == 0x00 ? int32_t(r.get(rs(instr))) < 0
                                        : int32_t(r.get(rs(instr))) >= 0);
        }
        case 0x02:
        case 0x03: {
            FlowResult f;
            f.flow = Flow::Taken;
            f.target = ((pc + 4u) & 0xF0000000u) | ((instr & 0x03FFFFFFu) << 2);
            return f;
        }
        default: return FlowResult{};
    }
}

inline int exec_muldiv(uint32_t instr, Regs& r) {
    switch (funct(instr)) {
        case 0x10: r.set(rd(instr), r.hi); return 1;
        case 0x11: r.hi = r.get(rs(instr)); return 1;
        case 0x12: r.set(rd(instr), r.lo); return 1;
        case 0x13: r.lo = r.get(rs(instr)); return 1;
        case 0x18: {
            const int64_t p = int64_t(int32_t(r.get(rs(instr)))) *
                              int64_t(int32_t(r.get(rt(instr))));
            r.lo = uint32_t(p);
            r.hi = uint32_t(uint64_t(p) >> 32);
            return 5;
        }
        case 0x19: {
            const uint64_t p = uint64_t(r.get(rs(instr))) * uint64_t(r.get(rt(instr)));
            r.lo = uint32_t(p);
            r.hi = uint32_t(p >> 32);
            return 5;
        }
        case 0x1A: {
            const int32_t a = int32_t(r.get(rs(instr)));
            const int32_t b = int32_t(r.get(rt(instr)));
            if (b != 0) { r.lo = uint32_t(a / b); r.hi = uint32_t(a % b); }
            return 37;
        }
        case 0x1B: {
            const uint32_t a = r.get(rs(instr));
            const uint32_t b = r.get(rt(instr));
            if (b != 0) { r.lo = a / b; r.hi = a % b; }
            return 37;
        }
        default: return 0;
    }
}

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
    bool halted = false;

    void load_program(Bus& bus, const uint8_t* bytes, size_t n,
                      uint32_t base = kProgramBase) {
        bus.store_bytes(base, bytes, n);
    }
};

inline StepResult cpu_step(CpuState& cpu, Bus& bus) {
    const uint32_t pc = cpu.window.current_pc;
    const uint32_t instr = bus.read32(pc);
    int cost = 1;
    bool taken = false;
    uint32_t target = 0;
    bool handled = true;

    switch (opcode(instr)) {
        case 0x00: {
            switch (funct(instr)) {
                case 0x21: case 0x23: case 0x24: case 0x25: case 0x26:
                case 0x27: case 0x2A: case 0x2B:
                    exec_alu_r(instr, cpu.regs);
                    break;
                case 0x00: case 0x02: case 0x03: case 0x04: case 0x06: case 0x07:
                    exec_shifts(instr, cpu.regs);
                    break;
                case 0x08:
                    taken = true;
                    target = jump_target(cpu.regs.get(rs(instr)));
                    break;
                case 0x09: {
                    const uint32_t dest = cpu.regs.get(rs(instr));
                    cpu.regs.set(rd(instr) == 0 ? 31u : rd(instr), link_address(pc));
                    taken = true;
                    target = jump_target(dest);
                    break;
                }
                case 0x0C: case 0x0D:
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
        case 0x07: {
            const FlowResult f = exec_branch(instr, cpu.regs, pc);
            if (f.flow == Flow::Taken) {
                taken = true;
                target = f.target;
                if (opcode(instr) == 0x03) cpu.regs.set(31, link_address(pc));
            }
            break;
        }
        case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: {
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            switch (opcode(instr)) {
                case 0x20: cpu.regs.set(rt(instr), do_load_byte(bus, addr, true)); break;
                case 0x21: cpu.regs.set(rt(instr), do_load_half(bus, addr, true)); break;
                case 0x23: cpu.regs.set(rt(instr), do_lw(bus, addr)); break;
                case 0x24: cpu.regs.set(rt(instr), do_load_byte(bus, addr, false)); break;
                case 0x25: cpu.regs.set(rt(instr), do_load_half(bus, addr, false)); break;
            }
            break;
        }
        case 0x22: case 0x26: {
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            const uint32_t merged =
                opcode(instr) == 0x26 ? do_lwr(bus, addr, cpu.regs.get(rt(instr)))
                                      : do_lwl(bus, addr, cpu.regs.get(rt(instr)));
            cpu.regs.set(rt(instr), merged);
            break;
        }
        case 0x28: case 0x29: case 0x2B: {
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            switch (opcode(instr)) {
                case 0x28: do_sb(bus, addr, cpu.regs.get(rt(instr))); break;
                case 0x29: do_sh(bus, addr, cpu.regs.get(rt(instr))); break;
                case 0x2B: do_sw(bus, addr, cpu.regs.get(rt(instr))); break;
            }
            break;
        }
        case 0x2A: case 0x2E: {
            const uint32_t addr = cpu.regs.get(rs(instr)) + sext16(instr);
            if (opcode(instr) == 0x2A) do_swl(bus, addr, cpu.regs.get(rt(instr)));
            else do_swr(bus, addr, cpu.regs.get(rt(instr)));
            break;
        }
        default:
            if (!exec_alu_i(instr, cpu.regs)) handled = false;
            break;
    }

    if (!handled) cpu.halted = true;

    if (!cpu.halted) {
        cpu.window = advance(cpu.window, taken, target);
        cpu.cycles += uint64_t(cost);
    }
    return StepResult{cpu.window.current_pc, cpu.window.next_pc,
                      cpu.window.in_delay_slot, cpu.cycles};
}

}  // namespace psx::r3000a
