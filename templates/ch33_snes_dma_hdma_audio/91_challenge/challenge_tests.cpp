// Tests for the challenge glue (bundle config -> channels -> effect buffer).
// The full acceptance criterion (golden hash via the runner) is documented
// in CHALLENGE.md.
#define LABSTEST_MAIN
#include "labstest.hpp"
#include "challenge.hpp"
#include <cstddef>

#include <array>

using namespace snesdma::challenge;

namespace {

std::vector<LineEffect> simple_log() {
    // Line 0: write 5; lines 1-2: silent (hold); line 3: write 9 on a
    // different register; line 4: watched register again with 2.
    return {
        {0, 0, 0x2100, 5},
        {3, 1, 0x2101, 9},
        {4, 0, 0x2100, 2},
    };
}

}  // namespace

TEST(ChallengeConfig, ParsesChannelsAndWatch) {
    const std::string config =
        "# gradient fixture\n"
        "watch=2100\n"
        "ch0.enable=1\n"
        "ch0.reg=2100\n"
        "ch0.regs=1\n"
        "ch0.indirect=0\n"
        "ch0.table=0000:01c0\n";
    uint16_t watch = 0;
    auto ch = parse_channels(config, watch);
    { EXPECT_TRUE(ch.size() == size_t(kChannels)); return; }
    EXPECT_EQ(watch, uint16_t(0x2100));
    EXPECT_TRUE(ch[0].enabled);
    EXPECT_FALSE(ch[0].indirect);
    EXPECT_EQ(ch[0].base_reg, uint16_t(0x2100));
    EXPECT_EQ(ch[0].regs_per_line, 1);
    EXPECT_EQ(ch[0].table_off, size_t(0));
    EXPECT_EQ(ch[0].table_len, size_t(0x1c0));
    EXPECT_FALSE(ch[7].enabled);  // untouched default
}

TEST(ChallengeConfig, IndirectChannelFields) {
    const std::string config =
        "watch=2101\n"
        "ch1.enable=1\n"
        "ch1.reg=2101\n"
        "ch1.regs=2\n"
        "ch1.indirect=1\n"
        "ch1.bank=7f\n"
        "ch1.table=0010:0014\n";
    uint16_t watch = 0;
    auto ch = parse_channels(config, watch);
    EXPECT_EQ(watch, uint16_t(0x2101));
    EXPECT_TRUE(ch[1].enabled);
    EXPECT_TRUE(ch[1].indirect);
    EXPECT_EQ(ch[1].bank, uint8_t(0x7F));
    EXPECT_EQ(ch[1].table_off, size_t(0x10));
    EXPECT_EQ(ch[1].table_len, size_t(0x14));
}

TEST(ChallengeBuffer, TracksWatchedRegisterPerLine) {
    std::array<uint8_t, kVisibleLines> buf{};
    build_effect_buffer(simple_log(), 0x2100, buf);
    EXPECT_EQ(buf[0], uint8_t(5));
    EXPECT_EQ(buf[1], uint8_t(5));  // held through silent lines
    EXPECT_EQ(buf[2], uint8_t(5));
    EXPECT_EQ(buf[3], uint8_t(5));  // line 3 wrote $2101, not watched
    EXPECT_EQ(buf[4], uint8_t(2));  // new value from line 4 onward
    EXPECT_EQ(buf[kVisibleLines - 1], uint8_t(2));
}

TEST(ChallengeBuffer, EmptyLogStaysZero) {
    std::array<uint8_t, kVisibleLines> buf{};
    build_effect_buffer({}, 0x2100, buf);
    for (const auto b : buf) EXPECT_EQ(b, uint8_t(0));
}
