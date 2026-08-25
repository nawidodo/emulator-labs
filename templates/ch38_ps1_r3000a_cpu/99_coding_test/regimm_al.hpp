#pragma once
#include <cstdint>
#include "alu.hpp"

namespace psx::r3000a {

enum class Flow : uint8_t { None, Taken };
struct FlowResult {
    Flow flow = Flow::Taken;
    uint32_t target = 0;
};

// Field extraction and Regs come from alu.hpp; only the two destination
// helpers (not part of the base ALU header) are defined here.
inline uint32_t link_address(uint32_t pc)    { return pc + 8u; }
inline uint32_t branch_target(uint32_t pc, int32_t disp) {
    return pc + 4u + uint32_t(disp << 2);
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// REGIMM linking variants: BLTZAL (rt=0x10) and BGEZAL (rt=0x11).
//
// MIPS I subtlety the hidden tests pin: $ra is written UNCONDITIONALLY —
// even when the branch is NOT taken — because R3000A computes the link in
// the decode stage before the condition resolves. Real PS1 compiler output
// relies on this.
inline bool exec_regimm_link(uint32_t instr, Regs& r, uint32_t pc,
                             FlowResult& out) {
    if (opcode(instr) != 0x01) return false;
    const uint32_t sel = rt(instr);
    const bool is_bltzal = sel == 0x10;
    const bool is_bgezal = sel == 0x11;
    if (!is_bltzal && !is_bgezal) return false;

    r.set(31, link_address(pc));  // unconditional link
    const int32_t val = static_cast<int32_t>(r.get(rs(instr)));
    if (is_bltzal ? val < 0 : val >= 0) {
        out.flow = Flow::Taken;
        out.target = branch_target(pc, int32_t(int16_t(imm16(instr))));
    } else {
        out.flow = Flow::None;
    }
    return true;
}
//@LABS-STUB
// TODO(1): implement BLTZAL (REGIMM rt=0x10) and BGEZAL (rt=0x11).
//   1. Reject anything that is not op=0x01 with rt in {0x10, 0x11} (return false).
//   2. Write $ra = pc + 8 FIRST — unconditionally, taken or not (MIPS I rule).
//   3. Take the branch when rs is negative (BLTZAL) or >= 0 (BGEZAL),
//      setting out.flow/out.target with displacement relative to pc+4.
// Return true when handled.
inline bool exec_regimm_link(uint32_t instr, Regs& r, uint32_t pc,
                             FlowResult& out) {
    (void)instr;
    (void)r;
    (void)pc;
    (void)out;
    return false;  // TODO(1)
}
//@LABS-END

}  // namespace psx::r3000a
