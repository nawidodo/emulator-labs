#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include "rewind.hpp"

TEST(rle, round_trip) {
    const std::vector<uint8_t> in{5, 5, 5, 0, 0, 9, 9, 9, 9, 1};
    const auto comp = snap::rle_compress(in);
    EXPECT_EQ(comp, (std::vector<uint8_t>{3, 5, 2, 0, 4, 9, 1, 1}));
    std::vector<uint8_t> out;
    EXPECT_TRUE(snap::rle_decompress(comp, out));
    EXPECT_EQ(out, in);
}

TEST(rle, long_runs_split_at_255) {
    std::vector<uint8_t> in(600, 7);
    in.push_back(2);
    const auto comp = snap::rle_compress(in);
    // 255+255+90 sevens then a single 2.
    EXPECT_EQ(comp.size(), size_t{8});
    std::vector<uint8_t> out;
    EXPECT_TRUE(snap::rle_decompress(comp, out));
    EXPECT_EQ(out, in);
}

TEST(rle, compresses_zero_frames_hard) {
    std::vector<uint8_t> blank(2048, 0);  // cleared 64x32 framebuffer
    EXPECT_TRUE(snap::rle_compress(blank).size() < size_t{20});
}

TEST(rle, corrupt_stream_rejected) {
    std::vector<uint8_t> out;
    const std::vector<uint8_t> odd{1, 2, 3};
    const std::vector<uint8_t> zcount{0, 9};
    EXPECT_FALSE(snap::rle_decompress(odd, out));   // odd length
    EXPECT_FALSE(snap::rle_decompress(zcount, out));  // zero count
}

TEST(rewind, ring_keeps_newest_capacity_entries) {
    snap::Ring ring(3);
    for (uint8_t i = 0; i < 5; ++i) {
        uint8_t v = i;
        ring.push(std::span(&v, 1));
    }
    EXPECT_EQ(ring.capacity(), size_t{3});
    EXPECT_EQ(ring.available(), size_t{3});
    // Newest (n=0) is state "4"; oldest kept is "2".
    EXPECT_EQ(*ring.step_back(0), std::vector<uint8_t>{4});
    EXPECT_EQ(*ring.step_back(1), std::vector<uint8_t>{3});
    EXPECT_EQ(*ring.step_back(2), std::vector<uint8_t>{2});
    EXPECT_EQ(ring.step_back(3), std::nullopt);  // evicted with slot 0/1
}

TEST(rewind, step_back_restores_exact_state) {
    // Capture every 10 frames of the bouncer-style pattern; verify that
    // stepping back restores the exact prior bytes.
    snap::Ring ring(8);
    std::vector<std::vector<uint8_t>> history;
    std::vector<uint8_t> state(64);
    for (int f = 0; f < 80; ++f) {
        state[size_t(f % 64)] ^= uint8_t(0xFF);
        if (f % 10 == 9) {
            ring.push(state);
            history.push_back(state);
        }
    }
    const auto restored = ring.step_back(3);
    EXPECT_TRUE(restored.has_value());
    EXPECT_EQ(*restored, history[history.size() - 4]);
}
