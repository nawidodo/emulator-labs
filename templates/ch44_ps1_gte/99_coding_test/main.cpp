// ct_nclip_tests — public unit tests + fixture mode for the coding test.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "labstest.hpp"
#include "nclip.hpp"

using gte::Cop2;

namespace {
std::string run_fixture_line(Cop2& g) {
    gtect::nclip(g);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "mac0=%d flag=%08X",
                  static_cast<int32_t>(g.rd(24)), g.flag());
    return buf;
}
}  // namespace

TEST(nclip, collinear_triangle_is_zero) {
    Cop2 g;
    g.wd(12, (100u << 16) | 200u);   // SXY0: sx=200 sy=100
    g.wd(13, (300u << 16) | 400u);   // SXY1: sx=400 sy=300
    g.wd(14, (500u << 16) | 600u);   // SXY2: sx=600 sy=500
    gtect::nclip(g);
    EXPECT_EQ(static_cast<int32_t>(g.rd(24)), 0);
    EXPECT_EQ(g.flag() & (gte::kFlagMac0PosOvf | gte::kFlagMac0NegOvf), 0u);
}

TEST(nclip, hand_computed_cross_product) {
    Cop2 g;
    // A=(0,0) B=(10,0) C=(0,10):
    //   0*0 + 10*10 + 0*0 - 0*10 - 10*0 - 0*0 = 100
    g.wd(12, 0u);
    g.wd(13, (0u << 16) | 10u);
    g.wd(14, (10u << 16) | 0u);
    gtect::nclip(g);
    EXPECT_EQ(static_cast<int32_t>(g.rd(24)), 100);
}

TEST(nclip, overflow_sets_flag_bits_and_clamps) {
    Cop2 g;
    // A=(-32768,-32768) B=(32767,0) C=(0,32767):
    //   wide sum = 3 * 1073709056 = 3221127168 > INT32_MAX -> overflow.
    g.wd(12, 0x80008000u);
    g.wd(13, 32767u);
    g.wd(14, 32767u << 16);
    gtect::nclip(g);
    EXPECT_EQ(static_cast<int32_t>(g.rd(24)), 0x7FFFFFFF);
    EXPECT_NE(g.flag() & gte::kFlagMac0PosOvf, 0u);
    EXPECT_NE(g.flag() & gte::kFlagMac0NegOvf, 0u);
    EXPECT_NE(g.flag() & 0x80000000u, 0u);           // ERROR aggregate
    EXPECT_EQ(g.flag() >> 16, g.flag() & 0xFFFFu);   // mirrored halves
}

int main(int argc, char** argv) {
    if (argc > 2) {
        // Fixture mode: inputs FILE, expectations FILE.
        std::ifstream in(argv[1]), exp(argv[2]);
        if (!in || !exp) {
            std::cerr << "error: cannot open fixtures\n";
            return 2;
        }
        std::string iline, eline;
        while (std::getline(in, iline)) {
            if (!std::getline(exp, eline)) {
                std::cerr << "error: expectation underrun\n";
                return 1;
            }
            if (iline.empty()) continue;
            std::istringstream ss(iline);
            int v[6];
            for (int& x : v)
                if (!(ss >> x)) {
                    std::cerr << "error: bad input line\n";
                    return 2;
                }
            Cop2 g;
            g.set_flag(gte::kFlagLmEcho);  // echo bits survive per spec
            g.wd(12, static_cast<uint32_t>(static_cast<uint16_t>(v[1])) << 16 |
                         static_cast<uint16_t>(v[0]));
            g.wd(13, static_cast<uint32_t>(static_cast<uint16_t>(v[3])) << 16 |
                         static_cast<uint16_t>(v[2]));
            g.wd(14, static_cast<uint32_t>(static_cast<uint16_t>(v[5])) << 16 |
                         static_cast<uint16_t>(v[4]));
            const std::string got = run_fixture_line(g);
            if (got != eline) {
                std::cout << "MISMATCH got='" << got << "' want='" << eline
                          << "'\n";
                return 1;
            }
        }
        std::cout << "all fixture lines match\n";
        return 0;
    }
    ::labstest::run_all("");
    return ::labstest::failures() == 0 ? 0 : 1;
}
