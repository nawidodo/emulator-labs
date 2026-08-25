#define LABSTEST_MAIN
#include "labstest.hpp"
#include "thumb_decoder.hpp"

using namespace thumb;

constexpr uint16_t f1(uint32_t op, uint32_t imm5, uint32_t rm, uint32_t rd) {
    return (op << 11) | (imm5 << 6) | (rm << 3) | rd;
}
constexpr uint16_t f2(bool imm, bool sub, uint32_t field, uint32_t rs, uint32_t rd) {
    return 0x1800 | ((imm ? 1u : 0u) << 10) | ((sub ? 1u : 0u) << 9) |
           (field << 6) | (rs << 3) | rd;
}
constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return (1u << 13) | (op << 11) | (rd << 8) | imm8;
}

TEST(decoder, format1_shifts) {
    const auto d = decode(f1(kLSL, 4, 2, 1));   // LSL r1, r2, #4
    EXPECT_EQ(d.fmt, kShift);
    EXPECT_EQ(d.op, kLSL);
    EXPECT_EQ(d.rd, 1u);
    EXPECT_EQ(d.rs, 2u);
    EXPECT_EQ(d.imm, 4u);
    const auto a = decode(f1(kASR, 31, 7, 0));
    EXPECT_EQ(a.op, kASR);
    EXPECT_EQ(a.imm, 31u);
}

TEST(decoder, format2_addsub) {
    const auto r = decode(f2(false, true, 0, 2, 0));  // SUBS r0, r0? fields: rn=0,rs=2,rd=0
    EXPECT_EQ(r.fmt, kAddSub);
    EXPECT_EQ(r.op, 1u);              // SUB
    EXPECT_FALSE(r.imm_form);
    EXPECT_EQ(r.rn, 0u);
    EXPECT_EQ(r.rs, 2u);
    const auto i = decode(f2(true, false, 5, 2, 0));  // ADDS r0, r2, #5
    EXPECT_EQ(i.fmt, kAddSub);
    EXPECT_TRUE(i.imm_form);
    EXPECT_EQ(i.rn, 5u);              // imm3 in the same field
}

TEST(decoder, format3_imm8_group) {
    const auto m = decode(f3(kF3MOV, 0, 0x42));  // MOV r0, #0x42
    EXPECT_EQ(m.fmt, kImmOp);
    EXPECT_EQ(m.op, kF3MOV);
    EXPECT_EQ(m.rd, 0u);
    EXPECT_EQ(m.imm, 0x42u);
    EXPECT_EQ(decode(f3(kF3CMP, 2, 200)).op, kF3CMP);
    EXPECT_EQ(decode(f3(kF3SUB, 7, 1)).op, kF3SUB);
}

TEST(decoder, format4_and_5) {
    // F4: 010000 op rs rd — op 12 = ORR? we only assert field extraction.
    const uint16_t hw = 0x4000 | (12u << 6) | (2u << 3) | 1u;
    const auto d = decode(hw);
    EXPECT_EQ(d.fmt, kAlu);
    EXPECT_EQ(d.op, 12u);
    EXPECT_EQ(d.rs, 2u);
    EXPECT_EQ(d.rd, 1u);

    const auto bx = decode(0x4718);   // BX r0? encoding: 010001 11 1 0 ...00 -> BX r0? use canonical BX r3 = 0x4718
    EXPECT_EQ(bx.fmt, kHiReg);
    EXPECT_EQ(bx.op, kBX);
    EXPECT_EQ(bx.rs, 3u);
    const auto cmp = decode(static_cast<uint16_t>(0x4580));  // CMP r8, r0
    EXPECT_EQ(cmp.fmt, kHiReg);
    EXPECT_EQ(cmp.op, kF5CMP);
}

TEST(decoder, format6_literal) {
    const auto d = decode(static_cast<uint16_t>(0x4800 | (2u << 8) | 0x08));
    EXPECT_EQ(d.fmt, kPcRel);
    EXPECT_EQ(d.rd, 2u);
    EXPECT_EQ(d.imm, 8u);             // *4 at execute -> +32 bytes
}

TEST(decoder, push_pop) {
    const auto p = decode(static_cast<uint16_t>(0xB500));  // PUSH {LR} = 1011 0101 0...
    EXPECT_EQ(p.fmt, kPush);
    EXPECT_EQ((p.imm & 0x100), 0x100u);   // LR bit
    const auto o = decode(static_cast<uint16_t>(0xBC00 | 0x100));  // POP {PC}
    EXPECT_EQ(o.fmt, kPop);
}

TEST(decoder, branches) {
    const auto b = decode(static_cast<uint16_t>(0xD0F2));  // BEQ -28? cond EQ imm8=0xF2
    EXPECT_EQ(b.fmt, kCondBranch);
    EXPECT_EQ(b.op, 0x0u);            // EQ
    EXPECT_EQ(branch_offset(b.imm, 8), -28);

    const auto u = decode(static_cast<uint16_t>(0xE7FE));  // B . (self)
    EXPECT_EQ(u.fmt, kBranch);
    EXPECT_EQ(branch_offset(u.imm, 11), -4);

    const auto hi = decode(static_cast<uint16_t>(0xF000));  // BL first: 11110
    EXPECT_EQ(hi.fmt, kBlFirst);
    const auto lo = decode(static_cast<uint16_t>(0xF800));  // BL second: 11111
    EXPECT_EQ(lo.fmt, kBlSecond);
    const auto lo2 = decode(static_cast<uint16_t>(0xFC01));
    EXPECT_EQ(lo2.fmt, kBlSecond);

    // Sign extension of the 11-bit unconditional offset.
    EXPECT_EQ(branch_offset(0x400, 11), -2048);  // bit10 set: negative
    EXPECT_EQ(branch_offset(0x401, 11), -2046);  // *2 scaling
}

TEST(hidden, decoder_hidden_formats) {
    // Sweep format boundaries for misclassification.
    for (uint32_t hw = 0; hw < 0x10000; ++hw) {
        Decoded d;
        const bool any =
            decode_data(hw, d) || decode_reg(hw, d) || decode_mem(hw, d) ||
            decode_branch(hw, d);
        if (!any) continue;
        switch (d.fmt) {
        case kShift: {
            EXPECT_EQ(hw >> 13, 0u);
            break;
        }
        case kImmOp: {
            EXPECT_EQ(hw >> 13, 1u);
            break;
        }
        case kCondBranch: {
            EXPECT_EQ(hw >> 12, 0xDu);
            break;
        }
        default:
            break;
        }
    }
}
