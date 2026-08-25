#pragma once
//
// ch44 / 03_lighting — NCDS, MVMVA and AVSZ3 on top of the RTPS core.
//
// Documented arithmetic (psx-spx aligned; goldens generated from this):
//
// NCDS (op 05h), SF shift applies to both matrix products:
//   step 1: MACi = Lrow_i . V                 (rotate the normal)
//           IRi  = sat16(MACi, LM)
//   step 2: IRi  = sat16(LCMrow_i . [IR1,IR2,IR3], LM)
//   step 3: t   = RBK*4096 + R*IR1 + G*IR2 + B*IR3   (material RGBC)
//           MAC0-slot = t ; IR0 = clamp(t>>12, 0, 32767)
//   step 4: MACi = FC_i*4096 + IR0*C_i
//           channel = clamp(MACi>>12, 0, 255) -> returned RGB
//
// MVMVA (op 12h): matrix select (cmd bits 17..16): 0=rotation, 1=light,
// 2/3=no multiply (documented subset). Vector select (15..14): 0=V0,
// 1=[IR1,IR2,IR3], 2=[IR1,IR2,SZ3], 3=zeros. Bit 13 CLEAR adds TR*1000h
// per row (translation term). Results land in control words 24..26 of
// this teaching model (real hardware: MAC1..3); FLAG composed with SF/LM.
//
// AVSZ3 (op 1Bh): MAC0 = (ZSF3*(SZ1+SZ2+SZ3)) >> 2;
//                 IR0  = clamp(MAC0 >> 12, 0, 32767).

#include <cstdint>

#include "../02_rtps/gte.hpp"

namespace gte {

struct Vec3 {
    int32_t x, y, z;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Light-matrix rows pack like the rotation matrix but start at control
// word 8 (words 8..12 hold L11..L33).
inline int16_t light_lane(const Cop2& g, unsigned idx) {
    const uint32_t w = g.cr(8u + idx / 2u);
    return static_cast<int16_t>(idx % 2 == 0 ? (w & 0xFFFFu) : (w >> 16));
}

inline int16_t light_row_dot(const Cop2& g, unsigned row, Vec3 v,
                             int shift) {
    uint32_t flags = 0;
    const int64_t sum = int64_t(light_lane(g, row * 3 + 0)) * v.x +
                        int64_t(light_lane(g, row * 3 + 1)) * v.y +
                        int64_t(light_lane(g, row * 3 + 2)) * v.z;
    return static_cast<int16_t>(mac_lane(sum, shift, flags));
}
//@LABS-STUB
// TODO(1): dot light-matrix `row` with v in wide math and shift by SF.
// Elements pack two lanes per control word starting at word 8, low half
// first (see light_lane above).
int16_t light_row_dot(const Cop2& g, unsigned row, Vec3 v, int shift) {
    (void)g; (void)row; (void)v; (void)shift;
    return 0;  // wrong on purpose
}
//@LABS-END

struct NcdsResult {
    int32_t mac0, mac1, mac2, mac3;
    int16_t ir0, ir1, ir2, ir3;
    uint8_t r, g, b;
    uint32_t flag;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline NcdsResult ncds(Cop2& g, bool lm) {
    uint32_t flags = 0;
    const int shift = 12;

    // Step 1: rotate the normal by the LIGHT matrix.
    const Vec3 n{g.vx(0), g.vy(0), g.vz(0)};
    NcdsResult r{};
    r.ir1 = saturate_ir(light_row_dot(g, 0, n, shift), lm, flags);
    r.ir2 = saturate_ir(light_row_dot(g, 1, n, shift), lm, flags);
    r.ir3 = saturate_ir(light_row_dot(g, 2, n, shift), lm, flags);

    // Step 2: light-color matrix rows live at control words 15..20.
    auto lcm_lane = [&](unsigned idx) {
        const uint32_t w = g.cr(15u + idx / 2u);
        return static_cast<int16_t>(idx % 2 == 0 ? (w & 0xFFFFu)
                                                  : (w >> 16));
    };
    auto lcm_row = [&](unsigned row) {
        const int64_t sum = int64_t(lcm_lane(row * 3 + 0)) * r.ir1 +
                            int64_t(lcm_lane(row * 3 + 1)) * r.ir2 +
                            int64_t(lcm_lane(row * 3 + 2)) * r.ir3;
        return mac_lane(sum, shift, flags);
    };

    // Material color from the RGBC data register (R,G,B,code bytes).
    const uint32_t rgbc = g.rd(6);
    const int cm_r = rgbc & 0xFFu;
    const int cm_g = (rgbc >> 8) & 0xFFu;
    const int cm_b = (rgbc >> 16) & 0xFFu;

    // Step 3: background color accumulation -> IR0.
    const auto bg = [&](unsigned which) -> int32_t {
        return which == 0 ? static_cast<int16_t>(g.cr(13) & 0xFFFFu)
             : which == 1 ? static_cast<int16_t>(g.cr(13) >> 16)
                          : static_cast<int16_t>(g.cr(14) & 0xFFFFu);
    };
    const int64_t sum0 = int64_t(bg(0)) * 4096 +
                         int64_t(cm_r) * r.ir1 + int64_t(cm_g) * r.ir2 +
                         int64_t(cm_b) * r.ir3;
    r.mac0 = sum0 > 0x7FFFFFFFll ? 0x7FFFFFFF
           : sum0 < -0x80000000ll ? -0x80000000
                                  : static_cast<int32_t>(sum0);
    if (sum0 > 0x7FFFFFFFll) flags |= kFlagMac0PosOvf;
    if (sum0 < -0x80000000ll) flags |= kFlagMac0NegOvf;
    const int64_t i0 = r.mac0 >> 12;
    if (i0 > 32767 || i0 < 0) flags |= kFlagIrSatSigned;
    r.ir0 = static_cast<int16_t>(i0 > 32767 ? 32767 : i0 < 0 ? 0 : i0);

    // Step 4: far color + intensity * material color.
    const auto fc = [&](unsigned axis) { return g.cr(21u + axis); };
    const int64_t m1 = int64_t(fc(0)) * 4096 + int64_t(r.ir0) * cm_r;
    const int64_t m2 = int64_t(fc(1)) * 4096 + int64_t(r.ir0) * cm_g;
    const int64_t m3 = int64_t(fc(2)) * 4096 + int64_t(r.ir0) * cm_b;
    auto channel = [](int64_t m) {
        const int64_t v = m >> 12;
        return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
    };
    r.mac1 = static_cast<int32_t>(m1);
    r.mac2 = static_cast<int32_t>(m2);
    r.mac3 = static_cast<int32_t>(m3);
    r.r = channel(m1);
    r.g = channel(m2);
    r.b = channel(m3);

    // Architectural write-back: MAC0..3 (data 24..27), IR0..3 (8..11).
    g.wd(24, r.mac0 & 0xFFFFFFFFu);
    g.wd(25, r.mac1 & 0xFFFFFFFFu);
    g.wd(26, r.mac2 & 0xFFFFFFFFu);
    g.wd(27, r.mac3 & 0xFFFFFFFFu);
    g.wd(8, static_cast<uint16_t>(r.ir0));
    g.wd(9, static_cast<uint16_t>(r.ir1));
    g.wd(10, static_cast<uint16_t>(r.ir2));
    g.wd(11, static_cast<uint16_t>(r.ir3));
    g.set_flag(compose_flag(flags, true, lm));
    r.flag = g.flag();
    return r;
}
//@LABS-STUB
// TODO(2): implement NCDS exactly as documented in the file header:
// four steps, LM-aware saturation, FLAG composition, write-back of
// MAC0..3 (data regs 24..27) and IR0..3 (data regs 8..11).
NcdsResult ncds(Cop2& g, bool lm) {
    (void)g; (void)lm;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void mvmva(Cop2& g, const Command& cmd) {
    const int shift = cmd.sf ? 12 : 0;
    uint32_t flags = 0;

    Vec3 v{0, 0, 0};
    switch (cmd.vec) {
        case 0:
            v = {g.vx(0), g.vy(0), g.vz(0)};
            break;
        case 1:
            v = {static_cast<int16_t>(g.rd(9)),
                 static_cast<int16_t>(g.rd(10)),
                 static_cast<int16_t>(g.rd(11))};          // IR1..IR3
            break;
        case 2:
            v = {static_cast<int16_t>(g.rd(9)),
                 static_cast<int16_t>(g.rd(10)),
                 static_cast<int16_t>(g.rd(19) & 0xFFFFu)};  // IR1,IR2,SZ3
            break;
        default:
            break;                                          // zeros
    }

    auto elem = [&](unsigned mat, unsigned i) -> int16_t {
        if (mat == 0) {                    // rotation: words 0..4
            const uint32_t w = g.cr(i / 2u);
            return static_cast<int16_t>(i % 2 == 0 ? (w & 0xFFFFu)
                                                   : (w >> 16));
        }
        return light_lane(g, i);           // light: words 8..12
    };
    const bool use_mat = cmd.mat <= 1;     // documented subset

    const int64_t tr_term[3] = {
        cmd.add12 && use_mat ? int64_t(g.tr(0)) * 4096 : 0,
        cmd.add12 && use_mat ? int64_t(g.tr(1)) * 4096 : 0,
        cmd.add12 && use_mat ? int64_t(g.tr(2)) * 4096 : 0,
    };
    const int64_t s1 = tr_term[0] +
                       int64_t(elem(cmd.mat, 0)) * v.x +
                       int64_t(elem(cmd.mat, 1)) * v.y +
                       int64_t(elem(cmd.mat, 2)) * v.z;
    const int64_t s2 = tr_term[1] +
                       int64_t(elem(cmd.mat, 3)) * v.x +
                       int64_t(elem(cmd.mat, 4)) * v.y +
                       int64_t(elem(cmd.mat, 5)) * v.z;
    const int64_t s3 = tr_term[2] +
                       int64_t(elem(cmd.mat, 6)) * v.x +
                       int64_t(elem(cmd.mat, 7)) * v.y +
                       int64_t(elem(cmd.mat, 8)) * v.z;

    g.wc(24, mac_lane(s1, shift, flags));  // results land here in THIS
    g.wc(25, mac_lane(s2, shift, flags));  // teaching model (documented;
    g.wc(26, mac_lane(s3, shift, flags));  // real hw: MAC1..3 data regs)
    g.set_flag(compose_flag(flags, cmd.sf, cmd.lm));
}

inline void avsz3(Cop2& g) {
    uint32_t flags = 0;
    const int32_t zsf3 = static_cast<int32_t>(g.cr(29));
    const int64_t prod =
        int64_t(zsf3) * (int64_t(static_cast<uint16_t>(g.rd(17))) +
                         static_cast<uint16_t>(g.rd(18)) +
                         static_cast<uint16_t>(g.rd(19)));
    const int64_t m0 = prod >> 2;
    int32_t mac0 = static_cast<int32_t>(m0);
    if (m0 > 0x7FFFFFFFll) { flags |= kFlagMac0PosOvf; mac0 = 0x7FFFFFFF; }
    if (m0 < -0x80000000ll) { flags |= kFlagMac0NegOvf; mac0 = -0x80000000; }
    if (m0 > 0x7FFFFFFFll || m0 < -0x80000000ll ||
        ((mac0 >> 12) > 32767)) flags |= kFlagIrSatSigned;
    const int16_t ir0 = static_cast<int16_t>(
        mac0 >> 12 > 32767 ? 32767 : mac0 >> 12 < 0 ? 0 : mac0 >> 12);
    g.wd(24, mac0 & 0xFFFFFFFFu);
    g.wd(8, static_cast<uint16_t>(ir0));
    g.set_flag(compose_flag(flags, false, false));
}
//@LABS-STUB
// TODO(3): implement MVMVA operand selection and AVSZ3 exactly as
// documented in the file header (AVSZ3: MAC0 = ZSF3*sum(SZ1..3)>>2,
// IR0 = clamp(MAC0>>12, 0, 32767)).
void mvmva(Cop2& g, const Command& cmd) {
    (void)g; (void)cmd;
}
void avsz3(Cop2& g) {
    (void)g;
}
//@LABS-END

}  // namespace gte
