#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "session.hpp"

namespace {
const std::vector<uint8_t> kRom = [] {
    // bouncer.bin bytes (see tests/public/ch35.../programs/bouncer.asm.txt)
    std::vector<uint8_t> data(0x30, 0);
    const std::vector<uint8_t> prog = {
        0xA2, 0x30, 0x60, 0x00, 0x61, 0x00, 0x62, 0x3F, 0x63,
        0x1F, 0xD0, 0x15, 0x70, 0x03, 0x71, 0x02, 0x80, 0x12,
        0x81, 0x32, 0x12, 0x0A};
    data.insert(data.begin(), prog.begin(), prog.end());
    const std::vector<uint8_t> sprite{0xAA, 0x55, 0xAA, 0x55, 0xAA};
    while (data.size() < 0x30) data.push_back(0);
    data.insert(data.end(), sprite.begin(), sprite.end());
    return data;
}();
}  // namespace

TEST(challenge, rewind_needs_history) {
    challenge::RewindSession s(kRom);
    EXPECT_FALSE(s.rewind_seconds(10));  // nothing captured yet
    for (int f = 0; f < 100; ++f) s.advance();
    EXPECT_FALSE(s.rewind_seconds(10));  // only ~1s of history
}

TEST(challenge, rewind_ten_seconds_equals_full_replay) {
    challenge::RewindSession straight(kRom);
    for (int f = 0; f < 700; ++f) straight.advance();

    challenge::RewindSession rewound(kRom);
    for (int f = 0; f < 700; ++f) rewound.advance();
    EXPECT_TRUE(rewound.state_hash() == straight.state_hash());
    EXPECT_TRUE(rewound.rewind_seconds(10));
    // Landed 600 frames back on the capture grid.
    for (int f = 0; f < 600; ++f) rewound.advance();
    // Resuming after a 10-second rewind converges bit-exactly.
    EXPECT_EQ(rewound.state_hash(), straight.state_hash());
}

TEST(challenge, ring_horizon_bounded) {
    challenge::RewindSession s(kRom);
    for (int f = 0; f < 5000; ++f) s.advance();  // way past capacity
    EXPECT_TRUE(s.history_depth() <= challenge::kRingCapacity);
    EXPECT_TRUE(s.rewind_seconds(10));   // full horizon still available
    EXPECT_FALSE(s.rewind_seconds(11));  // beyond the horizon: refused
}
