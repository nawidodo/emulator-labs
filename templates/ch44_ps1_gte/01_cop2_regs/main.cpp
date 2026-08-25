#define LABSTEST_MAIN
#include "labstest.hpp"
#include "cop2.hpp"

using namespace gte;

TEST(cop2, command_decode) {
    // 01h RTPS with SF=1, LM=0: word = (1<<10) | 0x01.
    const auto rtps = decode_command(0x00000401u);
    EXPECT_EQ(rtps.op, 0x01u);
    EXPECT_TRUE(rtps.sf);
    EXPECT_FALSE(rtps.lm);

    // NCDS with SF=1, LM=1.
    const auto ncds = decode_command(0x00000C85u);
    EXPECT_EQ(ncds.op, 0x05u);
    EXPECT_TRUE(ncds.lm);

    // MVMVA matrix/vector selects live at bits 17..16 / 15..14 and a
    // CLEAR bit 13 means "add the translation/background term".
    const auto mvmva = decode_command((0u << 16) | (1u << 14) | 0x12u);
    EXPECT_EQ(mvmva.op, 0x12u);
    EXPECT_EQ(mvmva.mat, 0u);
    EXPECT_EQ(mvmva.vec, 1u);
    EXPECT_TRUE(mvmva.add12);
}

TEST(cop2, saturate_ir_signed_and_unsigned) {
    uint32_t flags = 0;
    EXPECT_EQ(saturate_ir(100, false, flags), 100);
    EXPECT_EQ(flags, 0u);

    flags = 0;
    EXPECT_EQ(saturate_ir(40000, false, flags), 32767);   // signed clamp
    EXPECT_EQ(flags & kFlagIrSatSigned, kFlagIrSatSigned);

    flags = 0;
    EXPECT_EQ(saturate_ir(-40000, true, flags), 0);       // LM unsigned
    EXPECT_EQ(flags & kFlagIrSatUnsigned, kFlagIrSatUnsigned);

    flags = 0;
    EXPECT_EQ(saturate_ir(-40000, false, flags), -32768);
    EXPECT_EQ(flags & kFlagIrSatSigned, kFlagIrSatSigned);
}

TEST(cop2, compose_flag_echoes_and_mirrors) {
    // No error bits: only echoes + mirror.
    uint32_t f = compose_flag(kFlagMacPosOvf | kFlagIrSatSigned, true, false);
    EXPECT_NE(f & kFlagError, 0u);        // any error raises aggregate
    EXPECT_NE(f & kFlagSfEcho, 0u);
    EXPECT_EQ(f & kFlagLmEcho, 0u);
    EXPECT_EQ(f >> 16, f & 0xFFFFu);      // mirrored halves

    // No errors -> ERROR stays clear even though echoes set.
    f = compose_flag(0, false, true);
    EXPECT_EQ(f & kFlagError, 0u);
    EXPECT_NE(f & kFlagLmEcho, 0u);
}

TEST(cop2, packed_matrix_and_vector_accessors) {
    Cop2 g;
    g.set_rot(0, 0, 4096);              // R11 = 1.0
    g.set_rot(0, 1, -2048);             // R12 = -0.5
    EXPECT_EQ(g.rot(0, 0), 4096);
    EXPECT_EQ(g.rot(0, 1), -2048);
    // Setting one lane must not disturb its packed neighbour.
    g.set_rot(0, 2, 777);
    EXPECT_EQ(g.rot(0, 0), 4096);
    EXPECT_EQ(g.rot(0, 1), -2048);

    g.wd(0, 0x00000000u);
    EXPECT_EQ(g.vx(0), 0);
    EXPECT_EQ(g.vy(0), 0);
    g.wd(0, static_cast<uint32_t>(static_cast<uint16_t>(-123)) | (456u << 16));
    EXPECT_EQ(g.vx(0), -123);
    EXPECT_EQ(g.vy(0), 456);

    g.set_tr(0, -987654);
    EXPECT_EQ(g.tr(0), -987654);
}
