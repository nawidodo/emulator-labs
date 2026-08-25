#pragma once
// ch39 / 91_boot_mini — a miniature R3000A core sufficient to boot our
// synthetic BIOS stub headless: enough ALU/load/store/branch coverage to run
// an exception prologue, COP0 moves, an rfe-based return, and a self-loop
// halt.
//
// Delay-slot model follows the ch38 reference discipline: the interpreter
// explicitly tracks (pc, next_pc); a taken branch rewrites next_pc so the
// following fetch IS the delay slot, and any fault taken while executing
// that slot reports CAUSE.BD=1 with EPC pointing at the branch.
//
// Reuses the chapter building blocks: cop0.hpp (SR shadows, rfe) and
// bus.hpp (segments, RAM/scratchpad/BIOS decode).

#include <cstdint>
#include <optional>

#include "../01_cop0_regs/cop0.hpp"
#include "../02_exception_entry/exception.hpp"
#include "../03_memory_map/bus.hpp"

namespace psx::r3000a {

constexpr uint32_t kResetVector = 0xBFC00000u;

struct StepEvent {
    bool trapped = false;
    ExcCode code = ExcCode::Interrupt;
    bool bd = false;
    uint32_t epc = 0;
    uint32_t vector = 0;
};

class BootMini {
public:
    Bus bus;
    Cop0 cop0;
    uint32_t gpr[32] = {};
    uint32_t pc = kResetVector;

    void reset() {
        cop0.reset();
        for (auto& r : gpr) r = 0;
        pc = kResetVector;
        next_pc_ = pc + 4;
        in_delay_slot_ = false;
        branch_pc_ = 0;
    }

    bool in_delay_slot() const { return in_delay_slot_; }

    // True while the NEXT step() would execute a committed branch's delay
    // slot; pending_branch_pc() is that branch's address. Exposed so an
    // interrupt controller (99_coding_test) can preempt a slot instruction
    // with correct BD/EPC reporting.
    bool will_execute_delay_slot() const { return in_delay_slot_; }
    uint32_t pending_branch_pc() const { return branch_pc_; }

    // Force an exception as if the instruction at `fault_pc` (typically the
    // next fetch address) trapped. Used for externally asserted interrupts.
    StepEvent raise_exception(uint32_t fault_pc, ExcCode code) {
        return trap({}, fault_pc, in_delay_slot_, branch_pc_, code);
    }

    // Executes ONE instruction. Traps (syscall, reserved opcode, bad
    // address, ...) update COP0 and redirect fetch to the exception vector
    // inside this call — the caller only sees the StepEvent for tracing.
    StepEvent step() {
        StepEvent ev;
        const uint32_t at = pc;
        const bool slot = in_delay_slot_;
        const uint32_t baddr = branch_pc_;
        in_delay_slot_ = false;
        branch_pc_ = 0;

        uint32_t word = 0;
        if (!bus_read(&bus, at, &word))
            return trap(ev, at, slot, baddr, ExcCode::BusErrorInstruction);

        // Advance the pair FIRST so branch handlers only rewrite next_pc_.
        pc = next_pc_;
        next_pc_ = pc + 4;

        const uint32_t op = word >> 26;
        const uint32_t rs = (word >> 21) & 0x1F;
        const uint32_t rt = (word >> 16) & 0x1F;
        const uint32_t funct = word & 0x3F;
        const uint16_t imm = static_cast<uint16_t>(word & 0xFFFF);

        switch (op) {
            case 0x00:  // SPECIAL
                switch (funct) {
                    case 0x00:  // SLL (also the canonical NOP encoding)
                        set((word >> 11) & 0x1F, gpr[rt] << shamt(word));
                        break;
                    case 0x08:  // JR
                        do_jump(gpr[rs], at);
                        break;
                    case 0x0C:  // SYSCALL
                        return trap(ev, at, slot, baddr, ExcCode::Syscall);
                    default:
                        return trap(ev, at, slot, baddr,
                                    ExcCode::ReservedInstruction);
                }
                break;
            case 0x01:  // REGIMM
                if (rt == 0x11) {  // BGEZAL — assembler mnemonic "bal"
                    gpr[31] = at + 8;
                    if (static_cast<int32_t>(gpr[rs]) >= 0)
                        do_branch(at, imm);
                } else if (rt == 0x00) {  // BLTZ — used by CAUSE dispatchers
                    if (static_cast<int32_t>(gpr[rs]) < 0)
                        do_branch(at, imm);
                } else {
                    return trap(ev, at, slot, baddr,
                                ExcCode::ReservedInstruction);
                }
                break;
            case 0x04:  // BEQ
                if (gpr[rs] == gpr[rt]) do_branch(at, imm);
                break;
            case 0x09:  // ADDIU
                set(rt, gpr[rs] + static_cast<uint32_t>(sign_extend16(imm)));
                break;
            case 0x0D:  // ORI (zero-extended immediate)
                set(rt, gpr[rs] | imm);
                break;
            case 0x0F:  // LUI
                set(rt, static_cast<uint32_t>(imm) << 16);
                break;
            case 0x10:  // COP0
//@LABS-BEGIN 3
//@LABS-SOLUTION
                if (funct == 0x10) {  // RFE: pop ONE SR shadow level; the
                                      // handler's jr does the actual jump
                    cop0.sr = apply_rfe(cop0.sr);
                    break;
                }
                {
                    Cop0Move m{};
                    if (!decode_cop0_move(word, &m))
                        return trap(ev, at, slot, baddr,
                                    ExcCode::CoprocessorUnusable);
                    if (m.is_mtc0)
                        cop0.write(m.reg, gpr[m.gpr]);
                    else
                        set(m.gpr, cop0.read(m.reg));
                }
                break;
//@LABS-STUB
                // TODO(3): handle funct==0x10 as RFE (apply_rfe on SR),
                // dispatch MFC0/MTC0 register moves via decode_cop0_move,
                // and raise CoprocessorUnusable for unknown COP0 words.
                break;
//@LABS-END
            case 0x23:  // LW
            case 0x2B: {  // SW
                const uint32_t addr =
                    gpr[rs] + static_cast<uint32_t>(sign_extend16(imm));
                if (addr & 3u)
                    return trap(ev, at, slot, baddr,
                                op == 0x23 ? ExcCode::AddressErrorLoad
                                           : ExcCode::AddressErrorStore);
                if (op == 0x23) {
                    uint32_t v = 0;
                    if (!bus_read(&bus, addr, &v))
                        return trap(ev, at, slot, baddr,
                                    ExcCode::BusErrorData);
                    set(rt, v);
                } else {
                    if (!bus_write(&bus, addr, gpr[rt]))
                        return trap(ev, at, slot, baddr,
                                    ExcCode::BusErrorData);
                }
                break;
            }
            default:
                return trap(ev, at, slot, baddr,
                            ExcCode::ReservedInstruction);
        }
        return ev;
    }

private:
    uint32_t next_pc_ = pc + 4;
    bool in_delay_slot_ = false;
    uint32_t branch_pc_ = 0;

    static constexpr int32_t sign_extend16(uint16_t v) {
        return static_cast<int16_t>(v);
    }
    void set(uint32_t idx, uint32_t v) {
        if (idx != 0) gpr[idx] = v;  // $zero is hardwired
    }
    static uint32_t shamt(uint32_t word) { return (word >> 6) & 0x1F; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // All control transfer rewrites next_pc_: the instruction fetched next
    // (already sitting in pc) IS the delay slot, and we remember the branch
    // address so a fault in that slot can report BD=1/EPC=branch.
    void do_branch(uint32_t at, uint16_t imm16) {
        next_pc_ = at + 4 + (static_cast<uint32_t>(sign_extend16(imm16)) << 2);
        begin_delay_slot(at);
    }
    void do_jump(uint32_t target, uint32_t at) {
        next_pc_ = target;
        begin_delay_slot(at);
    }
    void begin_delay_slot(uint32_t at) {
        in_delay_slot_ = true;
        branch_pc_ = at;
    }
//@LABS-STUB
    // TODO(1): resolve branch/jump targets relative to the branch address
    // (target = at+4 + sign_extended_offset*4 for branches) and mark that
    // the following instruction executes in the branch's delay slot.
    void do_branch(uint32_t at, uint16_t imm16) {
        (void)at; (void)imm16;  // wrong on purpose: never branches
    }
    void do_jump(uint32_t target, uint32_t at) {
        (void)target; (void)at;  // wrong on purpose: falls through
    }
    void begin_delay_slot(uint32_t) {}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // Commit an exception: update CAUSE/EPC/SR via take_exception(), then
    // redirect fetch to the returned vector. `at` is the faulting
    // instruction; when it executed in a delay slot the request carries the
    // branch address so EPC lands on the branch and CAUSE.BD gets set.
    StepEvent trap(StepEvent ev, uint32_t at, bool slot, uint32_t baddr,
                   ExcCode code) {
        ExceptionRequest req{};
        req.pc = at;
        req.in_delay_slot = slot;
        if (slot) req.branch_pc = baddr;
        req.code = code;
        const ExceptionResult r = take_exception(&cop0, req);
        ev.trapped = true;
        ev.code = code;
        ev.bd = (r.cause & CAUSE_BD) != 0;
        ev.epc = r.epc;
        ev.vector = r.vector;
        pc = r.vector;
        next_pc_ = r.vector + 4;
        return ev;
    }
//@LABS-STUB
    // TODO(2): build the ExceptionRequest (branch address when in a delay
    // slot!), commit it through take_exception(), and redirect the fetch
    // pair (pc/next_pc_) to the resulting vector. Fill ev with
    // trapped/code/bd/epc/vector for the trace.
    StepEvent trap(StepEvent ev, uint32_t at, bool slot, uint32_t baddr,
                   ExcCode code) {
        (void)at; (void)slot; (void)baddr; (void)code;
        ev.trapped = false;  // wrong on purpose: exceptions go nowhere
        return ev;
    }
//@LABS-END
};

}  // namespace psx::r3000a
