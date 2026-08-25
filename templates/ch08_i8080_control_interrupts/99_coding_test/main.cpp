#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstddef>

#include "diverge.hpp"
#include "labstest.hpp"

// Chapter 8 coding test binary.
//
//   no args:                          run the labstest suites
//   --check GOLDEN ACTUAL EXPECTED:   exit 0 iff first_divergence(GOLDEN,
//                                     ACTUAL) == EXPECTED (hidden grader)

using namespace labsdiv;

namespace {

const char* kGolden =
    "pc=0000 op=31 af=0002 bc=0000 de=0000 hl=0000 sp=0000 cyc=0\n"
    "pc=0003 op=CD af=0102 bc=0000 de=0000 hl=2000 sp=2000 cyc=10\n"
    "pc=0008 op=3C af=0102 bc=0000 de=0000 hl=2000 sp=1FFE cyc=27\n"
    "pc=0009 op=C9 af=0206 bc=0000 de=0000 hl=2000 sp=1FFE cyc=32\n";

const char* kBuggyCycles =
    // same instructions, but the call burned the wrong cycles
    "pc=0000 op=31 af=0002 bc=0000 de=0000 hl=0000 sp=0000 cyc=0\n"
    "pc=0003 op=CD af=0102 bc=0000 de=0000 hl=2000 sp=2000 cyc=10\n"
    "pc=0008 op=3C af=0102 bc=0000 de=0000 hl=2000 sp=1FFE cyc=21\n"
    "pc=0009 op=C9 af=0206 bc=0000 de=0000 hl=2000 sp=1FFE cyc=26\n";

const char* kBuggyReturn =
    // return landed on a byte-swapped address
    "pc=0000 op=31 af=0002 bc=0000 de=0000 hl=0000 sp=0000 cyc=0\n"
    "pc=0003 op=CD af=0102 bc=0000 de=0000 hl=2000 sp=2000 cyc=10\n"
    "pc=0008 op=3C af=0102 bc=0000 de=0000 hl=2000 sp=1FFE cyc=27\n"
    "pc=8000 op=00 af=0206 bc=0000 de=0000 hl=2000 sp=2000 cyc=37\n";

std::vector<Fields> rows(const char* text) {
    std::istringstream in(text);
    return parse_trace(in);
}

}  // namespace

TEST(parse_line, splits_tokens_and_rejects_garbage) {
    Fields f;
    EXPECT_TRUE(parse_line("pc=0005 op=CD cyc=17", f));
    EXPECT_EQ(f.size(), size_t(3));
    EXPECT_EQ(f.at("op"), std::string("CD"));
    EXPECT_FALSE(parse_line("no tokens here", f));
    EXPECT_FALSE(parse_line("", f));
}

TEST(parse_trace, skips_blank_lines) {
    std::istringstream in("a=1\n\nb=2\n\n");
    auto r = parse_trace(in);
    EXPECT_EQ(r.size(), size_t(2));
    EXPECT_EQ(r[1].at("b"), std::string("2"));
}

TEST(first_divergence, identical_traces_yield_zero) {
    EXPECT_EQ(first_divergence(rows(kGolden), rows(kGolden)), 0);
}

TEST(first_divergence, finds_cycle_drift_at_the_call) {
    // The bug is on line 3 (the instruction AFTER the call consumed wrong
    // cumulative cycles): first divergence = 3.
    EXPECT_EQ(first_divergence(rows(kGolden), rows(kBuggyCycles)), 3);
}

TEST(first_divergence, finds_swapped_return_address) {
    EXPECT_EQ(first_divergence(rows(kGolden), rows(kBuggyReturn)), 4);
}

TEST(first_divergence, length_mismatch_is_a_divergence) {
    std::istringstream short_stream(kGolden);
    auto full = parse_trace(short_stream);
    std::vector<Fields> truncated(full.begin(), full.begin() + 2);
    EXPECT_EQ(first_divergence(full, truncated), 3);
}

// ---- hidden grader mode ----------------------------------------------

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--check") {
        const char* golden_path = argv[2];
        const char* actual_path = argv[3];
        const int expected = atoi(argv[4]);

        std::ifstream g(golden_path), a(actual_path);
        if (!g || !a) {
            std::cerr << "error: cannot open trace files\n";
            return 2;
        }
        const int got = first_divergence(parse_trace(g), parse_trace(a));
        if (got != expected) {
            std::printf("first divergence %d != expected %d\n", got,
                        expected);
            return 1;
        }
        return 0;
    }

    labstest::run_all(argc > 1 ? argv[1] : "");
    return labstest::failures() == 0 ? 0 : 1;
}
