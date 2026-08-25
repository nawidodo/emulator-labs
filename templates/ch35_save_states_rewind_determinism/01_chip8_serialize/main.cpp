#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "chip8.hpp"
#include "serialize.hpp"

namespace {
chip8::Machine demo_machine() {
    // 1x1 white pixel at (3,2) plus a couple of register values.
    chip8::Machine m;
    m.reset();
    m.fb[2 * chip8::kW + 3] = 1;
    m.v[5] = 0xAB;
    m.i = 0x123;
    m.dt = 7;
    return m;
}
}  // namespace

TEST(save, round_trip_preserves_everything) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    EXPECT_EQ(chip8::write_state(a, blob), chip8::kStateSize);

    chip8::Machine b;
    b.reset();
    EXPECT_TRUE(chip8::read_state(blob, b));
    EXPECT_EQ(b.v[5], uint8_t{0xAB});
    EXPECT_EQ(b.i, uint16_t{0x123});
    EXPECT_EQ(b.dt, uint8_t{7});
    EXPECT_EQ(b.pc, chip8::kPcStart);
    EXPECT_EQ(b.fb[2 * chip8::kW + 3], uint8_t{1});
    EXPECT_EQ(chip8::state_hash(a), chip8::state_hash(b));
}

TEST(save, version_byte_is_first) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(a, blob);
    EXPECT_EQ(blob[0], chip8::kStateVersion);
}

TEST(save, foreign_version_rejected) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(a, blob);
    blob[0] = 99;  // state from a hypothetical newer build
    chip8::Machine b;
    EXPECT_FALSE(chip8::read_state(blob, b));
}

TEST(save, short_buffer_rejected) {
    chip8::Machine a = demo_machine();
    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(a, blob);
    blob.pop_back();  // one byte short
    chip8::Machine b;
    EXPECT_FALSE(chip8::read_state(blob, b));
}

TEST(save, hash_tracks_mutations) {
    chip8::Machine a = demo_machine();
    chip8::Machine b = demo_machine();
    EXPECT_EQ(chip8::state_hash(a), chip8::state_hash(b));
    b.v[0] ^= 1;  // single-register change must move the hash
    EXPECT_NE(chip8::state_hash(a), chip8::state_hash(b));
}

TEST(save, rng_state_survives_round_trip) {
    // CXNN consumes the xorshift state; a state that loses rng desyncs on
    // the very next random op.
    const std::vector<uint8_t> rom = {
        0xC0, 0xFF,              // 200: V0 = rnd & FF
        0xF0, 0x33,              // 202: BCD V0 -> mem[I..]
        0x12, 0x00,              // 204: JP 200
    };
    chip8::Machine run_a, run_b;
    run_a.reset();
    run_b.reset();
    run_a.load(rom);
    run_b.load(rom);
    for (int f = 0; f < 10; ++f) run_a.frame();

    std::vector<uint8_t> blob(chip8::kStateSize);
    chip8::write_state(run_a, blob);
    EXPECT_TRUE(chip8::read_state(blob, run_b));
    for (int f = 0; f < 50; ++f) {
        run_b.frame();
    }
    for (int f = 0; f < 50; ++f) {
        run_a.frame();
    }
}
