#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstring>
#include <vector>

#include "abi_boundary.hpp"

using namespace chip8abi;

namespace {

constexpr int kFbSize = 64 * 32;

bool cfg_ok(int err) {
    EXPECT_EQ(err, CH8_OK);
    return err == CH8_OK;
}

Ch8Config valid_cfg() {
    Ch8Config c{};
    c.struct_size = sizeof(Ch8Config);
    c.abi_version = kAbiVersion;
    c.cycles_per_frame = 110;
    c.flags = 0;
    return c;
}

// ROM: V0=8 ; I=font ; draw 8x1 sprite at (V0,V0) ; DT=V0 ; HLT-ish loop.
const std::vector<uint8_t> kRom = {
    0x60, 0x08,              // 6XNN  V0 = 8
    0xA2, 0x0A,              // ANNN  I = 0x20A (sprite data below)
    0xD0, 0x11,              // DXYN  draw at (V0,V0), height=1
    0xF0, 0x15,              // FX15  DT = V0
    0x12, 0x00,              // JP 0x200 (idle spin)
    0x80,                    // sprite byte at 0x20A: pixel column 10000000
};

}  // namespace

TEST(abi, create_rejects_wrong_version_and_size) {
    Ch8Config c = valid_cfg();
    int err = -1;
    EXPECT_EQ(ch8_create(&c, &err) != nullptr, true);
    EXPECT_EQ(err, CH8_OK);

    c.abi_version = kAbiVersion + 1;
    err = -1;
    EXPECT_EQ(ch8_create(&c, &err), nullptr);
    EXPECT_EQ(err, CH8_ERR_VERSION);

    c = valid_cfg();
    c.struct_size = sizeof(Ch8Config) + 4;   // newer header than we know
    err = -1;
    EXPECT_EQ(ch8_create(&c, &err), nullptr);
    EXPECT_EQ(err, CH8_ERR_SIZE);
}

TEST(abi, load_rom_validates_size_and_rewinds_pc) {
    Ch8Config c = valid_cfg();
    int err = 0;
    Ch8Machine* m = ch8_create(&c, &err);
    EXPECT_EQ(err, CH8_OK);

    EXPECT_EQ(ch8_load_rom(m, nullptr, 4), CH8_ERR_NO_ROM);
    EXPECT_EQ(ch8_load_rom(m, kRom.data(), 0), CH8_ERR_NO_ROM);

    std::vector<uint8_t> big(kRomMaxBytes + 1, 0x00);
    EXPECT_EQ(ch8_load_rom(m, big.data(),
                           static_cast<uint16_t>(big.size())),
              CH8_ERR_ROM_TOO_BIG);        // stub silently truncates instead

    EXPECT_EQ(ch8_load_rom(m, kRom.data(),
                           static_cast<uint16_t>(kRom.size())), CH8_OK);
    ch8_destroy(m);
}

TEST(abi, run_frame_executes_and_ticks_timers) {
    Ch8Config c = valid_cfg();
    int err = 0;
    Ch8Machine* m = ch8_create(&c, &err);
    if (!cfg_ok(err)) return;
    EXPECT_EQ(ch8_load_rom(m, kRom.data(),
                           static_cast<uint16_t>(kRom.size())), CH8_OK);

    // One frame: program draws the sprite and sets DT=8.
    EXPECT_EQ(ch8_run_frame(m, 0), CH8_OK);
    uint8_t fb[kFbSize] = {};
    EXPECT_EQ(ch8_read_frame(m, fb), CH8_OK);
    EXPECT_EQ(fb[8 * 64 + 8], 1);          // pixel at (8,8)

    uint8_t dt = 99;
    EXPECT_EQ(ch8_read_delay_timer(m, &dt), CH8_OK);
    EXPECT_EQ(dt, 8);

    // Each later frame decrements DT once: 8 frames -> 0.
    for (int i = 0; i < 8; ++i) ch8_run_frame(m, 0);
    ch8_read_delay_timer(m, &dt);
    EXPECT_EQ(dt, 0);                      // stub leaves timers frozen

    ch8_destroy(m);
}

TEST(abi, read_frame_guards_null_args) {
    Ch8Config c = valid_cfg();
    int err = 0;
    Ch8Machine* m = ch8_create(&c, &err);
    uint8_t fb[kFbSize];
    EXPECT_EQ(ch8_read_frame(nullptr, fb), CH8_ERR_SIZE);
    EXPECT_EQ(ch8_read_frame(m, nullptr), CH8_ERR_SIZE);
    ch8_destroy(m);
}
