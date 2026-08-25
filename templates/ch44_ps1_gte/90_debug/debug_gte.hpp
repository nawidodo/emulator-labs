#pragma once
//
// ch44 / 90_debug — SEEDED BUGS in an RTPS-style transform.
//
// The unit tests run RED until both bugs are found. Write up each fix in
// bug-report.md (bug / root cause / first divergence / fix / regression).

#include <cstdint>

#include "../01_cop2_regs/cop2.hpp"

namespace gtedbg {

struct Result {
    int32_t mac1 = 0;
    int16_t ir1 = 0;
    uint16_t sz3 = 0;
    int32_t mac0 = 0;
    int32_t ir0 = 0;   // wide on purpose: divide-overflow forces 0x1FFFF
    int16_t sx2 = 0;
    uint32_t flag = 0;
};

inline Result project(gte::Cop2& g, bool lm) {
    uint32_t flags = 0;
    const int vx = g.vx(0), vy = g.vy(0), vz = g.vz(0);
    const auto row = [&](unsigned r) {
        return int64_t(g.tr(r)) * 4096 +
               int64_t(g.rot(r, 0)) * vx + int64_t(g.rot(r, 1)) * vy +
               int64_t(g.rot(r, 2)) * vz;
    };

    Result out;
    out.mac1 = static_cast<int32_t>(row(0) >> 12);
    // NOTE: wide-math overflow flags omitted in this teaching subset.

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Architectural IR write must honor LM: unsigned range 0..32767 when
    // LM=1, signed -32768..32767 otherwise.
    if (lm) {
        if (out.mac1 < 0) {
            out.ir1 = 0;
            flags |= gte::kFlagIrSatUnsigned;
        } else {
            out.ir1 = gte::saturate_ir(out.mac1, false, flags);
        }
    } else {
        out.ir1 = gte::saturate_ir(out.mac1, false, flags);
    }
//@LABS-STUB
    // TODO(1): symptom — commands run with LM=1 still produce negative
    // IR values; hardware clamps those to 0..32767 and raises bit 27.
    out.ir1 = gte::saturate_ir(out.mac1, false, flags);
//@LABS-END

    out.sz3 = static_cast<uint16_t>(
        out.mac1 < 0 ? 0 : (out.mac1 > 0xFFFF ? 0xFFFF : out.mac1));

//@LABS-BEGIN 2
//@LABS-SOLUTION
    if (out.sz3 == 0) {
        // Divide overflow: ERROR/divide flag MUST be raised and the
        // results forced to their documented maxima.
        flags |= gte::kFlagDivideOvf;
        out.mac0 = 0x1FFFF;
        out.ir0 = 0x1FFFF;
        out.sx2 = 1023;
    } else {
        const int64_t m0 =
            (((int64_t)g.h() * 0x20000) / out.sz3 + 1) / 2;
        out.mac0 = static_cast<int32_t>(m0 > 0x20000 ? 0x20000 : m0);
        out.ir0 = static_cast<int16_t>(out.mac0 >> 1);
        out.sx2 = static_cast<int16_t>(g.ofx() +
                                       ((int64_t)out.ir1 * out.mac0 >> 16));
    }
//@LABS-STUB
    // TODO(2): symptom — depth-zero vertices silently produce garbage
    // IR0/SX instead of the documented forced-max outputs, and FLAG never
    // reports the divide overflow (bit 31).
    {
        const int64_t m0 =
            (((int64_t)g.h() * 0x20000) / (out.sz3 == 0 ? 1 : out.sz3) + 1) /
            2;
        out.mac0 = static_cast<int32_t>(m0);
        out.ir0 = 0;  // BUG 2: wrong forced value + missing FLAG.31
        out.sx2 = 0;
    }
//@LABS-END

    g.set_flag(gte::compose_flag(flags, true, lm));
    out.flag = g.flag();
    return out;
}

}  // namespace gtedbg
