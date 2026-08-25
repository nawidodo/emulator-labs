#pragma once
// ch39 / 02_exception_entry — the R3000A exception entry algorithm.
//
// Curriculum reference design: exception entry UPDATES ARCHITECTURAL STATE,
// it does not jump. The caller (CPU core) receives back a target vector and
// continues fetching there; all semantics live in the state update:
//   CAUSE.ExcCode / CAUSE.BD / CAUSE.CE are recorded,
//   EPC records where to resume (branch address when BD=1),
//   SR slides its shadow pairs down (kernel mode, IRQs off),
//   vector base comes from SR.BEV.
//
// Vectors per nocash PSX-SPX "Exception Vectors" table:
//   General exception: BEV=0 -> 80000080h, BEV=1 -> BFC00180h.
//
// Reference: https://problemkaputt.de/psx-spx.htm#cpuexceptions

#include <cstdint>
#include <optional>

#include "../01_cop0_regs/cop0.hpp"

namespace psx::r3000a {

struct ExceptionRequest {
    uint32_t pc;              // address of the faulting instruction
    bool in_delay_slot;       // faulting instruction sits in a branch delay slot
    std::optional<uint32_t> branch_pc;  // required when in_delay_slot
    ExcCode code;
    uint32_t coprocessor = 0;  // CE field source for CpU faults (COP number)
};

struct ExceptionResult {
    uint32_t vector;  // fetch resumes here
    uint32_t cause;   // full CAUSE value after entry
    uint32_t epc;
    uint32_t sr;      // full SR value after the shadow push
};

inline constexpr uint32_t general_vector(bool bev) {
    return bev ? 0xBFC00180u : 0x80000080u;
}

// The whole entry sequence as one pure state transition. `cop0` is updated
// in place so the caller's register file stays authoritative.
inline ExceptionResult take_exception(Cop0* cop0, const ExceptionRequest& req) {
// BD is architectural state both fixes below depend on, so it is computed
// unconditionally outside the seeded blocks.
    const bool bd = req.in_delay_slot && req.branch_pc.has_value();

//@LABS-BEGIN 1
//@LABS-SOLUTION

    // EPC must point at the instruction to RETRY or SKIP. In a delay slot
    // only the branch address is recoverable (the slot address itself is not
    // saved anywhere), so EPC = branch and CAUSE.BD tells the handler that
    // resuming at EPC+4 would re-execute the branch.
    cop0->epc = bd ? *req.branch_pc : req.pc;
//@LABS-STUB
    // BUG(1): EPC records the delay-slot address instead of the branch.
    // The slot address is architecturally unreachable for resume (it is
    // gone once the branch takes), so an ERETL here resumes into the
    // middle of nowhere and the handler double-faults.
    cop0->epc = req.pc;   // wrong on purpose
//@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    cop0->cause = (cop0->cause & ~(CAUSE_EXCCODE_MASK | CAUSE_CE_MASK)) |
                  (static_cast<uint32_t>(req.code) << 2) |
                  ((req.coprocessor & 3u) << 28);
    if (bd)
        cop0->cause |= CAUSE_BD;
    else
        cop0->cause &= ~CAUSE_BD;

    cop0->sr = push_sr_on_exception(cop0->sr);
    //@LABS-STUB
    // BUG(2): the kernel-mode SR shadow push is missing. The handler runs
    // with the user-mode status word still live, so a subsequent ERET
    // pops the WRONG shadow and returns to user code with kernel bits
    // half-set - privilege state is silently corrupted.
    cop0->cause = (cop0->cause & ~(CAUSE_EXCCODE_MASK | CAUSE_CE_MASK)) |
                  (static_cast<uint32_t>(req.code) << 2) |
                  ((req.coprocessor & 3u) << 28);
    if (bd)
        cop0->cause |= CAUSE_BD;
    else
        cop0->cause &= ~CAUSE_BD;
    // push_sr_on_exception intentionally skipped  // wrong on purpose
    //@LABS-END
    return {general_vector((cop0->sr & SR_BEV) != 0), cop0->cause, cop0->epc,
            cop0->sr};
}

}  // namespace psx::r3000a
