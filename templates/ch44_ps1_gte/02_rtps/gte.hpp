#pragma once
//
// ch44 / 02_rtps — GTE rotation + perspective transform core.
// RTPS (op 01h) and RTPT (op 02h), psx-spx arithmetic rules:
//
//   [MAC1,2,3] = [TRX,TRY,TRZ]*10000h + R * [VX,VY,VZ]     44-bit sums
//   IR1..3     = saturate(MAC SAR sf*12)                   LM selects range
//   SZ3        = clamp(MAC3 >> sf_shift, 0, FFFFh)         depth FIFO push
//   MAC0       = min(((H*20000h / SZ3)+1)/2, 20000h)       SZ3==0 => FLAG.31
//   SX2/SY2    = clamp(OFX/OFY + (IR*MAC0)>>16, -1024, +1023)
//
// Mathematical intermediates are 64-bit; architectural saturation happens
// only at register write-back.

#include <cstdint>

#include "../01_cop2_regs/cop2.hpp"

namespace gte {

constexpr int64_t kMacMax = (int64_t{1} << 43) - 1;
constexpr int64_t kMacMin = -(int64_t{1} << 43);

//@LABS-BEGIN 1
//@LABS-SOLUTION
// One rotation row times a vector plus translation, in wide math, with
// the 44-bit overflow check applied BEFORE the SF shift.
inline int32_t mac_lane(int64_t sum, int shift, uint32_t& flags) {
    if (sum > kMacMax) { flags |= kFlagMacPosOvf; sum = kMacMax; }
    if (sum < kMacMin) { flags |= kFlagMacNegOvf; sum = kMacMin; }
    return static_cast<int32_t>(sum >> shift);
}
//@LABS-STUB
// TODO(1): clamp `sum` to the 44-bit MAC range raising FLAG bits 30/29,
// THEN shift right arithmetically by `shift` and truncate to int32.
int32_t mac_lane(int64_t sum, int shift, uint32_t& flags) {
    (void)sum; (void)shift; (void)flags;
    return 0;  // wrong on purpose
}
//@LABS-END

struct RtpsResult {
    int32_t mac0, mac1, mac2, mac3;
    int32_t ir0;  // wide: divide-overflow forces 0x1FFFF
    int16_t ir1, ir2, ir3;
    uint16_t sz3;
    int16_t sx2, sy2;
    uint32_t flag;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline RtpsResult rtps(Cop2& g, unsigned slot, bool lm = false) {
    uint32_t flags = 0;

    const int vx = g.vx(slot), vy = g.vy(slot), vz = g.vz(slot);
    const int shift = 12;  // this exercise always runs SF=1

    const int64_t s1 = int64_t(g.tr(0)) * 4096 +
                       int64_t(g.rot(0, 0)) * vx + int64_t(g.rot(0, 1)) * vy +
                       int64_t(g.rot(0, 2)) * vz;
    const int64_t s2 = int64_t(g.tr(1)) * 4096 +
                       int64_t(g.rot(1, 0)) * vx + int64_t(g.rot(1, 1)) * vy +
                       int64_t(g.rot(1, 2)) * vz;
    const int64_t s3 = int64_t(g.tr(2)) * 4096 +
                       int64_t(g.rot(2, 0)) * vx + int64_t(g.rot(2, 1)) * vy +
                       int64_t(g.rot(2, 2)) * vz;

    RtpsResult r{};
    r.mac1 = mac_lane(s1, shift, flags);
    r.mac2 = mac_lane(s2, shift, flags);
    r.mac3 = mac_lane(s3, shift, flags);
    r.ir1 = saturate_ir(r.mac1, lm, flags);
    r.ir2 = saturate_ir(r.mac2, lm, flags);
    r.ir3 = saturate_ir(r.mac3, lm, flags);
    // Note: this core applies LM to all IR lanes (psx-spx behavior).

    int32_t sz3 = r.mac3;
    if (sz3 < 0) sz3 = 0;
    if (sz3 > 0xFFFF) sz3 = 0xFFFF;
    r.sz3 = static_cast<uint16_t>(sz3);

    if (r.sz3 == 0) {
        // Divide overflow: ERROR bit set, results forced to max values.
        flags |= kFlagDivideOvf;
        r.mac0 = 0x1FFFF;
        r.ir0 = 0x1FFFF;
        r.sx2 = 1023;     // documented forced-max screen coords
        r.sy2 = 1023;
    } else {
        const int64_t m0raw =
            (((int64_t)g.h() * 0x20000) / r.sz3 + 1) / 2;
        r.mac0 = static_cast<int32_t>(
            m0raw > 0x20000 ? 0x20000 : m0raw);
        r.ir0 = static_cast<int16_t>(r.mac0 > 32767 ? 32767 : r.mac0);
        if (r.mac0 > 32767) flags |= kFlagIrSatSigned;

        const int dx = static_cast<int>((int64_t)r.ir1 * r.mac0 >> 16);
        const int dy = static_cast<int>((int64_t)r.ir2 * r.mac0 >> 16);
        auto sx_limit = [](int v) {
            return static_cast<int16_t>(v < -1024 ? -1024
                                      : v > 1023 ? 1023 : v);
        };
        r.sx2 = sx_limit(g.ofx() + dx);
        r.sy2 = sx_limit(g.ofy() + dy);
    }

    // Write-back to the architectural register file.
    g.wd(24, r.mac0 & 0xFFFFFFFFu);           // MAC0
    g.wd(25, r.mac1 & 0xFFFFFFFFu);           // MAC1..3
    g.wd(26, r.mac2 & 0xFFFFFFFFu);
    g.wd(27, r.mac3 & 0xFFFFFFFFu);
    g.wd(8, static_cast<uint16_t>(r.ir0));    // IR0
    g.wd(9, static_cast<uint16_t>(r.ir1));
    g.wd(10, static_cast<uint16_t>(r.ir2));
    g.wd(11, static_cast<uint16_t>(r.ir3));
    g.wd(19, r.sz3);                          // SZ3
    // Hardware RTPS pushes the screen FIFO: SXY0<=SXY1<=SXY2<=new.
    g.wd(12, g.rd(13));
    g.wd(13, g.rd(14));
    g.wd(14, static_cast<uint16_t>(r.sx2) |
                 uint32_t(static_cast<uint16_t>(r.sy2)) << 16);  // SXY2
    g.set_flag(compose_flag(flags, true, lm));
    r.flag = g.flag();
    return r;
}
//@LABS-STUB
// TODO(2): implement RTPS per the psx-spx rules in the file header,
// including the SZ3==0 divide-overflow path and SXY2 clamping to
// [-1024,+1023]. Write back MAC0..3 (data 24..27), IR0..3 (8..11),
// SZ3 (19), SXY2 (14) and FLAG (control 31).
RtpsResult rtps(Cop2& g, unsigned slot, bool lm = false) {
    (void)g; (void)slot; (void)lm;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// RTPT transforms three vertices; each RTPS already pushes the screen
// FIFO (SXY0<=SXY1<=SXY2<=result) and FLAG accumulates across steps.
inline void rtpt(Cop2& g) {
    uint32_t accumulated = 0;
    for (unsigned slot = 0; slot < 3; ++slot) {
        const RtpsResult r = rtps(g, slot);
        accumulated |= r.flag;  // FLAG is sticky across RTPT steps
    }
    g.set_flag(accumulated);
}
//@LABS-STUB
// TODO(3): run rtps() over vertices 0, 1 and 2 (the FIFO push happens
// inside rtps, matching hardware).
void rtpt(Cop2& g) {
    (void)g;
    // NOTE: real FLAG semantics are sticky across the three steps.
}
//@LABS-END

}  // namespace gte
