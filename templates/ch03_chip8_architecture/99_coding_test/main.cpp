#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "chip8.hpp"

using chip8::Chip8;

// Visible smoke tests only. The coding test is graded by hidden ROM-image
// cases (see CODING_TEST.md) — deliberately NO behavioral hints here.

TEST(coding_test, machine_boots) {
    Chip8 c;
    c.reset();
    EXPECT_EQ(c.pc(), 0x200);
    EXPECT_EQ(c.sp(), 0);
}

TEST(coding_test, base_ops_still_work) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x60, 0x42};  // LD V0, 0x42
    c.load(rom);
    c.step();
    EXPECT_EQ(c.v(0), 0x42);
}

TEST(coding_test, stubs_do_not_crash_the_runner_path) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x22, 0x0A, 0x00, 0xEE, 0x30,
                           0x42, 0x40, 0x42, 0xB2, 0x00};
    c.load(rom);
    for (int n = 0; n < 5; ++n) c.step();  // must not crash; semantics hidden
}
