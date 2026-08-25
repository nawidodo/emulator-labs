#define LABSTEST_MAIN
#include "labstest.hpp"
#include "conditions.hpp"

using arm::FLAG_C;
using arm::FLAG_N;
using arm::FLAG_V;
using arm::FLAG_Z;

// Independent oracle written straight from the ARM ARM table so the tests do
// not just mirror the implementation's structure.
static bool oracle(uint32_t cond, uint32_t cpsr) {
    const bool n = cpsr & FLAG_N, z = cpsr & FLAG_Z;
    const bool c = cpsr & FLAG_C, v = cpsr & FLAG_V;
    switch (cond) {
    case 0x0: return z;                    // EQ
    case 0x1: return !z;                   // NE
    case 0x2: return c;                    // CS
    case 0x3: return !c;                   // CC
    case 0x4: return n;                    // MI
    case 0x5: return !n;                   // PL
    case 0x6: return v;                    // VS
    case 0x7: return !v;                   // VC
    case 0x8: return c && !z;              // HI
    case 0x9: return !c || z;              // LS
    case 0xA: return n == v;               // GE
    case 0xB: return n != v;               // LT
    case 0xC: return !z && n == v;         // GT
    case 0xD: return z || n != v;          // LE
    case 0xE: return true;                 // AL
    default:  return false;                // NV
    }
}

TEST(conditions, all_16_x_all_flag_combos) {
    for (uint32_t cond = 0; cond < 16; ++cond) {
        for (uint32_t flags = 0; flags < 16; ++flags) {
            const uint32_t cpsr = ((flags & 1) ? FLAG_V : 0) |
                                  ((flags & 2) ? FLAG_C : 0) |
                                  ((flags & 4) ? FLAG_Z : 0) |
                                  ((flags & 8) ? FLAG_N : 0);
            EXPECT_EQ(arm::cond_pass(cond, cpsr), oracle(cond, cpsr));
        }
    }
}

TEST(conditions, unsigned_family_spot_checks) {
    EXPECT_TRUE(arm::cond_unsigned(0x0, true, false));   // EQ needs Z
    EXPECT_TRUE(arm::cond_unsigned(0x3, false, false));  // CC needs !C
    EXPECT_TRUE(arm::cond_unsigned(0x8, false, true));   // HI: C && !Z
    EXPECT_FALSE(arm::cond_unsigned(0x8, true, true));
    EXPECT_TRUE(arm::cond_unsigned(0x9, true, false));   // LS: !C || Z
    EXPECT_FALSE(arm::cond_unsigned(0x9, false, true));
}

TEST(conditions, signed_family_spot_checks) {
    EXPECT_TRUE(arm::cond_signed(0xA, true, true, false));   // GE N==V
    EXPECT_FALSE(arm::cond_signed(0xB, true, true, false));  // LT N!=V
    EXPECT_TRUE(arm::cond_signed(0xC, false, false, false)); // GT
    EXPECT_FALSE(arm::cond_signed(0xC, false, false, true)); // GT blocked by Z
    EXPECT_TRUE(arm::cond_signed(0xD, false, false, true));  // LE holds on Z
}

TEST(conditions, misc_family_and_nv) {
    EXPECT_TRUE(arm::cond_misc(0xE, false, false));  // AL always executes
    EXPECT_FALSE(arm::cond_misc(0xF, true, true));   // NV never executes
    EXPECT_TRUE(arm::cond_misc(0x6, false, true));   // VS
}

TEST(hidden, cond_table_hidden) {
    // Hidden grader suite: same exhaustive sweep as above.
    for (uint32_t cond = 0; cond < 16; ++cond)
        for (uint32_t flags = 0; flags < 16; ++flags) {
            const uint32_t cpsr = flags << 28;
            EXPECT_EQ(arm::cond_pass(cond, cpsr), oracle(cond, cpsr));
        }
}
