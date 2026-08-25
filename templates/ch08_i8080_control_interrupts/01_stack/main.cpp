#define LABSTEST_MAIN
#include "labstest.hpp"

#include "stack.hpp"

using namespace i8080;

namespace {

struct Rig {
    FlatBus bus;
    uint16_t sp = 0;
};

}  // namespace

TEST(push, downward_predecrement) {
    Rig rig;
    rig.sp = 0x2000;
    push(rig.bus, rig.sp, 0x1234);
    EXPECT_EQ(rig.sp, 0x1FFE);
    EXPECT_EQ(rig.bus.mem[0x1FFF], 0x12);  // high at SP-1
    EXPECT_EQ(rig.bus.mem[0x1FFE], 0x34);  // low at SP-2
}

TEST(push, two_values_stack_up) {
    Rig rig;
    rig.sp = 0x2000;
    push(rig.bus, rig.sp, 0xAAAA);
    push(rig.bus, rig.sp, 0xBBBB);
    EXPECT_EQ(rig.sp, 0x1FFC);
    EXPECT_EQ(rig.bus.mem[0x1FFE], 0xAA);   // first push sits deeper
    EXPECT_EQ(rig.bus.mem[0x1FFD], 0xBB);   // second push on top
}

TEST(pop, reverses_push_exactly) {
    Rig rig;
    rig.sp = 0x2000;
    push(rig.bus, rig.sp, 0xC3D4);
    const uint16_t v = pop(rig.bus, rig.sp);
    EXPECT_EQ(v, 0xC3D4);
    EXPECT_EQ(rig.sp, 0x2000);  // balanced
}

TEST(pop, byte_order_is_low_first) {
    Rig rig;
    rig.bus.mem[0x3000] = 0x78;   // low
    rig.bus.mem[0x3001] = 0x56;   // high
    rig.sp = 0x3000;
    // NOTE: labstest's EXPECT_EQ double-evaluates its arguments, so never
    // put a mutating expression directly inside one.
    const uint16_t v = pop(rig.bus, rig.sp);
    EXPECT_EQ(v, 0x5678);
    EXPECT_EQ(rig.sp, 0x3002);
}

TEST(push_pop, psw_layout_roundtrip) {
    // PUSH PSW puts A in the high byte, packed flags in the low byte.
    const uint8_t f = pack_psw(true, false, true, true, true);
    EXPECT_EQ(f & FLAG_S, FLAG_S);
    EXPECT_FALSE(f & FLAG_Z);
    EXPECT_EQ(f & FLAG_AC, FLAG_AC);
    EXPECT_TRUE(f & FLAG_P);
    EXPECT_TRUE(f & FLAG_CY);
    EXPECT_EQ(f & 0x02, 0x02);      // bit 1 always set
    EXPECT_EQ(f & 0x08, 0);         // bit 3 always clear
    EXPECT_EQ(f & 0x20, 0);         // bit 5 always clear

    const FlagsView v = unpack_psw(f);
    EXPECT_TRUE(v.s);
    EXPECT_FALSE(v.z);
    EXPECT_TRUE(v.ac);
    EXPECT_TRUE(v.p);
    EXPECT_TRUE(v.cy);

    Rig rig;
    rig.sp = 0x1000;
    push(rig.bus, rig.sp, uint16_t(uint16_t(0xAB) << 8 | f));
    EXPECT_EQ(rig.bus.mem[0x0FFF], 0xAB);
    EXPECT_EQ(rig.bus.mem[0x0FFE], f);
}

TEST(psw_pack, all_sixteen_combinations_survive) {
    for (unsigned m = 0; m < 32; ++m) {
        const bool s = m & 16, z = m & 8, ac = m & 4, p = m & 2, cy = m & 1;
        const FlagsView v = unpack_psw(pack_psw(s, z, ac, p, cy));
        EXPECT_TRUE(v.s == s && v.z == z && v.ac == ac && v.p == p &&
                    v.cy == cy);
    }
}
