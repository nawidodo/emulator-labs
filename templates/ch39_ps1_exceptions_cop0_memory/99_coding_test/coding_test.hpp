#pragma once
// ch39 / 99_coding_test — an UNSEEN exception sequence: a timed interrupt
// that preempts (a) a branch delay slot in program code and (b) the `jr; rfe`
// return sequence of an active handler, forcing a genuinely nested exception
// with two levels of SR shadow push.
//
// The student implements the interrupt controller glue below:
//   deliverable() — CAUSE.IP8 && SR.Im bit8 && SR.IEc (PSX-SPX: IP bits 8-9
//                   are R/W; "as long as any of the bits are set they will
//                   cause an interrupt if the corresponding bit is set in IM")
//   step_irq()    — assert IP8 on its scheduled cycle, then either deliver an
//                   Interrupt exception (BD/EPC aware via the BootMini slot
//                   state) or execute the next instruction.
//
// Reference: https://problemkaputt.de/psx-spx.htm#cpuexceptions

#include "../91_challenge/boot_mini.hpp"

namespace psx::r3000a {

// CAUSE bit 8 is software-interrupt request IP8 (one of the two R/W bits);
// its enable line is SR.Im bit 8 (the Im field occupies SR bits 15:8).
constexpr uint32_t kCauseIpSwInterrupt = 0x100u;
constexpr uint32_t kSrImBit8 = 0x100u;


class NestedCpu : public BootMini {
public:
    long irq_cycle_a = -1;
    long irq_cycle_b = -1;
    long cycles_run = 0;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    bool deliverable() const {
        return (cop0.cause & kCauseIpSwInterrupt) != 0 &&
               (cop0.sr & kSrImBit8) != 0 && (cop0.sr & SR_IEC) != 0;
    }
//@LABS-STUB
    // TODO(1): an interrupt is deliverable iff CAUSE.IP8 is set AND the
    // matching SR.Im mask bit is set AND SR.IEc enables interrupts.
    bool deliverable() const { return false; }  // wrong on purpose
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    StepEvent step_irq() {
        ++cycles_run;
        if (cycles_run == irq_cycle_a || cycles_run == irq_cycle_b)
            cop0.cause |= kCauseIpSwInterrupt;  // line asserts mid-stream

        if (deliverable()) {
            // The preempted instruction is whatever fetches next; when it
            // sits in a committed branch's delay slot, raise_exception()
            // reports BD=1 with EPC pointing at that branch.
            return raise_exception(pc, ExcCode::Interrupt);
        }
        return step();
    }
//@LABS-STUB
    // TODO(2): advance the cycle counter, assert CAUSE.IP8 on the scheduled
    // cycles, and when deliverable() force an Interrupt exception at the
    // pending fetch address (pc). Otherwise run one normal instruction.
    StepEvent step_irq() {
        StepEvent ev;  // wrong on purpose: does nothing
        return ev;
    }
//@LABS-END
};

}  // namespace psx::r3000a
