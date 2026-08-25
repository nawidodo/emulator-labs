#pragma once
//
// ch44 / 99_coding_test — unseen-spec GTE opcode: NCLIP (op 04h).
// Specification in CODING_TEST.md. Hidden grading feeds screen-coordinate
// triples your build has never seen.

#include <cstdint>

#include "../02_rtps/gte.hpp"

namespace gtect {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int32_t nclip(const gte::Cop2& g, uint32_t& flag_out) {
    const auto sx = [&](unsigned r) {
        return static_cast<int16_t>(g.rd(12 + r) & 0xFFFFu);
    };
    const auto sy = [&](unsigned r) {
        return static_cast<int16_t>(g.rd(12 + r) >> 16);
    };
    const int64_t m0 = int64_t(sx(0)) * sy(1) + int64_t(sx(1)) * sy(2) +
                       int64_t(sx(2)) * sy(0) - int64_t(sx(0)) * sy(2) -
                       int64_t(sx(1)) * sy(0) - int64_t(sx(2)) * sy(1);
    int32_t mac0 = static_cast<int32_t>(m0);
    if (m0 > 0x7FFFFFFFll || m0 < -0x80000000ll) {
        flag_out |= gte::kFlagMac0PosOvf | gte::kFlagMac0NegOvf;
        mac0 = m0 > 0 ? 0x7FFFFFFF : static_cast<int32_t>(-0x80000000ll);
    } else if (m0 == static_cast<int64_t>(-0x80000000ll)) {
        flag_out |= gte::kFlagMac0NegOvf;
    }
    // FLAG: keep SF/LM echoes, OR in the new MAC0 overflow bits, raise
    // ERROR when any error bit present, mirror halves.
    const uint32_t prev = g.flag();
    uint32_t f =
        (prev & (gte::kFlagSfEcho | gte::kFlagLmEcho)) |
        (flag_out & (gte::kFlagMac0PosOvf | gte::kFlagMac0NegOvf));
    if (f & ((1u << 20) | (1u << 21))) f |= 1u << 31;
    f |= f >> 16;
    flag_out = f;
    return mac0;
}

inline void nclip(gte::Cop2& g) {
    uint32_t raw = 0;
    const int32_t m0 = nclip(g, raw);
    g.wd(24, static_cast<uint32_t>(m0));
    g.set_flag(raw);
}
//@LABS-STUB
// TODO(1): implement NCLIP per CODING_TEST.md. The stub computes nothing
// and never raises flags — wrong on purpose.
int32_t nclip(const gte::Cop2& g, uint32_t& flag_out) {
    (void)g; (void)flag_out;
    return 0;
}
void nclip(gte::Cop2& g) {
    (void)g;
}
//@LABS-END

}  // namespace gtect
