#pragma once
#include <cstdint>

#include "si_constants.hpp"

// Exercise 4 — the dual one-shot vblank timers.
//
// The board generates its interrupt cadence with two one-shot timers
// wired to alternate: even frames jam RST 08 onto the data bus during
// INTA, odd frames jam RST 10. One shot fires per frame period, so a
// program that leaves interrupts enabled sees the sequence
//
//   RST 08, RST 10, RST 08, RST 10, ...
//
// one vector per 1/60 s frame. A masked CPU (IFF clear) simply loses the
// pulse — these are edge one-shots, not level latches.

namespace si {

struct IrqRaise {
    bool raised = false;
    uint8_t opcode = 0;
};

class VblankTimers {
public:
    VblankTimers() { configure(kCyclesPerFrame, kIrqOpcodeEven, kIrqOpcodeOdd); }

    void configure(uint64_t cycles_per_frame, uint8_t opcode_even,
                   uint8_t opcode_odd) {
        cpf_ = cycles_per_frame;
        op_even_ = opcode_even;
        op_odd_ = opcode_odd;
        reset();
    }

    void reset() {
        next_fire_ = cpf_;
        even_frame_ = true;
    }

    // Called before every CPU step with the cumulative cycle count.
    // Reports at most one raise per call — no instruction can span a
    // whole frame period.
    IrqRaise poll(uint64_t cycles_now) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        IrqRaise r;
        if (cycles_now >= next_fire_) {
            r.raised = true;
            r.opcode = even_frame_ ? op_even_ : op_odd_;
            even_frame_ = !even_frame_;
            next_fire_ += cpf_;
        }
        return r;
//@LABS-STUB
        // TODO(1): fire when `cycles_now` reaches `next_fire_`: return the
        // alternating opcode (even frames first), flip the parity for next
        // time and push the deadline out by one frame period.
        (void)cycles_now;
        return IrqRaise{};
//@LABS-END
    }

    uint64_t next_fire() const { return next_fire_; }
    bool even_frame() const { return even_frame_; }

private:
    uint64_t cpf_ = kCyclesPerFrame;
    uint64_t next_fire_ = cpf_;
    bool even_frame_ = true;
    uint8_t op_even_ = kIrqOpcodeEven;
    uint8_t op_odd_ = kIrqOpcodeOdd;
};

}  // namespace si
