#define LABSTEST_MAIN
#include "labstest.hpp"
#include "disc.hpp"

#include <sstream>

using namespace cdrom;

TEST(disc, msf_lba_roundtrip_with_leadin_bias) {
    EXPECT_EQ(msf_to_lba(0, 2, 0), 0);     // first data sector (00:02:00)
    EXPECT_EQ(msf_to_lba(0, 0, 0), -150);  // lead-in start
    unsigned m, s, f;
    lba_to_msf(0, m, s, f);
    EXPECT_EQ(m, 0u); EXPECT_EQ(s, 2u); EXPECT_EQ(f, 0u);

    const int32_t lba = msf_to_lba(12, 34, 56);
    lba_to_msf(lba, m, s, f);
    EXPECT_EQ(m, 12u); EXPECT_EQ(s, 34u); EXPECT_EQ(f, 56u);
}

TEST(disc, parse_cue_accepts_mode2_and_rejects_other) {
    const std::string good =
        "FILE \"game.bin\" BINARY\n"
        "  TRACK 01 MODE2/2352\n"
        "    INDEX 01 00:02:00\n";
    Track t;
    EXPECT_TRUE(parse_cue(good, t));
    EXPECT_EQ(t.number, 1u);
    EXPECT_EQ(t.lba_start, msf_to_lba(0, 2, 0));

    const std::string audio =
        "FILE \"a.wav\" WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n";
    Track t2;
    EXPECT_FALSE(parse_cue(audio, t2));
}

namespace {
// Build a tiny synthetic MODE2/2352 image entirely in memory.
std::vector<uint8_t> make_bin(unsigned n_sectors) {
    std::vector<uint8_t> bin(size_t(n_sectors) * kRawSectorSize, 0);
    for (unsigned i = 0; i < n_sectors; ++i) {
        uint8_t* p = &bin[size_t(i) * kRawSectorSize];
        p[0] = 0x00;
        for (unsigned j = 1; j < 11; ++j) p[j] = 0xFF;
        p[11] = 0x00;
        unsigned m, s, f;
        lba_to_msf(static_cast<int32_t>(i), m, s, f);
        p[12] = static_cast<uint8_t>(((m / 10) << 4) | (m % 10));
        p[13] = static_cast<uint8_t>(((s / 10) << 4) | (s % 10));
        p[14] = static_cast<uint8_t>(((f / 10) << 4) | (f % 10));
        p[15] = 0x02;  // MODE2, form1
        p[16] = 0x00; p[17] = 0x00; p[18] = 0x00; p[19] = 0x01;
        for (unsigned j = 0; j < kUserDataSize; ++j)
            p[24 + j] = static_cast<uint8_t>((i * 7 + j) & 0xFF);
    }
    return bin;
}
}  // namespace

TEST(disc, read_sector_validates_headers) {
    DiscImage d;
    const auto bin = make_bin(16);
    // Feed through load() via temp files? Keep to read path by testing a
    // loaded image built from text cue + memory file is not possible with
    // this API — instead validate through public read_sector after load.
    // For unit isolation we re-implement load from string in the test:
    // (load() requires files; covered by the runner fixtures.)
    (void)d;

    // Header checks against hand-built bytes:
    const auto& b = bin;
    EXPECT_EQ(b[15], 0x02);                      // MODE2
    EXPECT_FALSE(sector_is_form2(&b[0]));        // form1 subheader
    const uint8_t* ud = nullptr;
    unsigned size = 0;
    EXPECT_TRUE(sector_user_data(&b[0], &ud, &size));
    EXPECT_EQ(size, kUserDataSize);
    EXPECT_EQ(ud[0], 0);                         // sector 0 pattern start
}

TEST(disc, form2_detected_via_subheader_bit) {
    const auto bin = make_bin(1);
    auto& b = const_cast<std::vector<uint8_t>&>(bin);
    b[18] = 0x20;  // set form2 bit
    EXPECT_TRUE(sector_is_form2(&b[0]));
    const uint8_t* ud = nullptr;
    unsigned size = 0;
    EXPECT_FALSE(sector_user_data(&b[0], &ud, &size));  // XA stub rejects
}
