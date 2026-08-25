#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "chip8.hpp"
#include "serialize.hpp"

namespace {
chip8::Machine demo_machine() {
    chip8::Machine m;
    m.reset();
    m.dt = 7;   // distinct values so a swap cannot hide
    m.st = 13;
    m.v[3] = 0x5A;
    return m;
}
}  // namespace

TEST(debug_save, round_trip_preserves_everything) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    EXPECT_EQ(chip8::write_state(a, blob), chip8::kStateSize);

    chip8::Machine b;
    b.reset();
    EXPECT_TRUE(chip8::read_state(blob, b));
    // RED with the seeded bug: dt/st come back swapped.
    EXPECT_EQ(b.dt, uint8_t{7});
    EXPECT_EQ(b.st, uint8_t{13});
    EXPECT_EQ(chip8::state_hash(a), chip8::state_hash(b));
}

TEST(debug_save, other_fields_still_round_trip) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(a, blob);
    chip8::Machine b;
    EXPECT_TRUE(chip8::read_state(blob, b));
    EXPECT_EQ(b.v[3], uint8_t{0x5A});
    EXPECT_EQ(b.pc, chip8::kPcStart);
}

TEST(debug_save, foreign_version_rejected) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(a, blob);
    blob[0] = 99;
    chip8::Machine b;
    EXPECT_FALSE(chip8::read_state(blob, b));
}
