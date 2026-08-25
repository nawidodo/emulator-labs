#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>

#include "frame_diff.hpp"

using namespace nes22diff;

namespace {

constexpr size_t kBytes = size_t(kW) * kH * 4;

std::string flat_frame(uint8_t r, uint8_t g, uint8_t b) {
    std::string f(size_t(kBytes), '\0');
    for (int i = 0; i < kW * kH; ++i) {
        f[i * 4] = char(r);
        f[i * 4 + 1] = char(g);
        f[i * 4 + 2] = char(b);
        f[i * 4 + 3] = char(0xFF);
    }
    return f;
}

// A left band (x<100) of color c on a gray background.
std::string band_frame(uint8_t c, int width) {
    std::string f = flat_frame(10, 10, 10);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < width && x < kW; ++x) {
            size_t o = (size_t(y) * kW + x) * 4;
            f[o] = char(c); f[o + 1] = char(c); f[o + 2] = char(c);
        }
    return f;
}

}  // namespace

TEST(nes22diff, identical_frames_report_no_diff) {
    std::string a = flat_frame(1, 2, 3);
    DiffReport r;
    count_diff(a, a, r);
    classify_shift(a, a, r);
    EXPECT_EQ(r.ndiff, 0);
    EXPECT_EQ(r.first_x, -1);
    EXPECT_EQ(std::string(r.shift), "none");
}

TEST(nes22diff, counts_and_locates_first_difference) {
    std::string a = band_frame(200, 100);
    std::string b = band_frame(200, 104);   // band 4px wider
    DiffReport r;
    count_diff(a, b, r);
    // Divergence starts at x=100 on every row; first in scan order is (100,0).
    EXPECT_EQ(r.ndiff, 4 * kH);
    EXPECT_EQ(r.first_x, 100);
    EXPECT_EQ(r.first_y, 0);
    EXPECT_NE(r.hash_a, r.hash_b);
}

TEST(nes22diff, classifies_one_pixel_horizontal_shift) {
    std::string a = band_frame(200, 50);
    std::string b = band_frame(200, 51);   // content grew by one column:
    // every differing pixel's color equals the neighbor column -> h1.
    DiffReport r;
    count_diff(a, b, r);
    classify_shift(a, b, r);
    EXPECT_EQ(std::string(r.shift), "h1");
}

TEST(nes22diff, classifies_one_pixel_vertical_shift) {
    // Top row colored, rest gray vs two top rows colored.
    auto frame_rows = [](int rows) {
        std::string f = flat_frame(9, 9, 9);
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < kW; ++x) {
                size_t o = (size_t(y) * kW + x) * 4;
                f[o] = char(180); f[o + 1] = char(40); f[o + 2] = char(40);
            }
        return f;
    };
    std::string a = frame_rows(5);
    std::string b = frame_rows(6);
    DiffReport r;
    count_diff(a, b, r);
    classify_shift(a, b, r);
    EXPECT_EQ(std::string(r.shift), "v1");
}

TEST(nes22diff, unexplainable_change_is_other) {
    std::string a = flat_frame(0, 0, 0);
    std::string b = flat_frame(0, 0, 0);
    b[100000] = char(255);   // single arbitrary byte flip
    DiffReport r;
    count_diff(a, b, r);
    classify_shift(a, b, r);
    EXPECT_EQ(r.ndiff, 1);
    EXPECT_EQ(std::string(r.shift), "other");
}
