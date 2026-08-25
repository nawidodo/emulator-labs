#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <cstring>

#include "chip8.hpp"

namespace {

using ch06::Chip8;
using ch06::Chip8Quirks;

struct FinalState {
    uint8_t v[16];
    uint16_t i;
};

FinalState run(const uint8_t* rom, size_t len, const Chip8Quirks& q) {
    Chip8 cpu;
    cpu.reset(q);
    cpu.load({rom, len});
    int guard = 0;
    while (!cpu.halted() && guard++ < 1000) cpu.step();
    FinalState f;
    std::memcpy(f.v, cpu.v, 16);
    f.i = cpu.i;
    return f;
}

// Save V3 = 0x12 at 0x400, then reload it. Written for a COSMAC VIP, where
// FX55/FX65 leave I untouched.
const uint8_t kSaveReload[] = {
    0x63, 0x12,  // 200: LD   V3, 0x12
    0xA4, 0x00,  // 202: LD   I, 0x400
    0xF3, 0x55,  // 204: LD   [I], V3
    0xF3, 0x65,  // 206: LD   V3, [I]     -> VIP: reloads 0x12
    0x33, 0x12,  // 208: SE   V3, 0x12    -> skips next on VIP
    0x60, 0xFF,  // 20A: LD   V0, 0xFF    <- symptom lands here
    0x12, 0x0E,  // 20C: JP   20E
};

}  // namespace

TEST(debug, fx55_must_leave_i_under_vip) {
    const auto f = run(kSaveReload, sizeof kSaveReload,
                       ch06::kCosmacVipQuirks);
    EXPECT_EQ(f.i, 0x400);
}

TEST(debug, save_reload_roundtrip_under_vip) {
    const auto f = run(kSaveReload, sizeof kSaveReload,
                       ch06::kCosmacVipQuirks);
    EXPECT_EQ(f.v[3], 0x12);  // the reloaded value must survive
    EXPECT_EQ(f.v[0], 0x00);  // ...so the SE at 208 must skip the LD V0
}

TEST(debug, modern_profile_still_advances_i_after_fx55) {
    const auto f = run(kSaveReload, sizeof kSaveReload,
                       ch06::kModernQuirks);
    EXPECT_EQ(f.i, 0x408);  // store +1 and load +4: I ends at 408
    EXPECT_EQ(f.v[3], 0x00);
}
