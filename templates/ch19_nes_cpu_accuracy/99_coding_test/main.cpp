// 99 — Coding test: locate the first divergence between two trace logs.
//
//   ch19_99_coding_test_tests [filter] [good.log] [bad.log]
//
// Without file arguments the committed 50k-line pair test skips; the
// hidden grader passes repo-relative paths to the committed fixtures.
#include "labstest.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* g_good = nullptr;
const char* g_bad = nullptr;

bool read_lines(const std::string& path, std::vector<std::string>* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        out->push_back(line);
    }
    return true;
}

void write_scratch(const char* path, const std::vector<std::string>& lines) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    for (const auto& l : lines) out << l << '\n';
}

constexpr const char* kScratchA = "ch19_ct_scratch_a.tmp.log";
constexpr const char* kScratchB = "ch19_ct_scratch_b.tmp.log";

}  // namespace

//@LABS-BEGIN 1
//@LABS-SOLUTION
/// Scan two log files line-by-line. On the FIRST difference store the
/// 1-based line number and both lines and return true. Two empty or
/// identical files return false with *line_no == 0. A length mismatch is
/// a divergence at the first missing line (missing side reports "<eof>").
inline bool find_first_divergence(const std::string& path_a,
                                  const std::string& path_b,
                                  uint64_t* line_no, std::string* line_a,
                                  std::string* line_b) {
    *line_no = 0;
    line_a->clear();
    line_b->clear();
    std::vector<std::string> a, b;
    if (!read_lines(path_a, &a) || !read_lines(path_b, &b)) return true;
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            *line_no = i + 1;
            *line_a = a[i];
            *line_b = b[i];
            return true;
        }
    }
    if (a.size() != b.size()) {          // one log ran longer
        *line_no = n + 1;
        *line_a = n < a.size() ? a[n] : "<eof>";
        *line_b = n < b.size() ? b[n] : "<eof>";
        return true;
    }
    return false;
}
//@LABS-STUB
// TODO(1): implement the scanner described above — read both files,
// compare line-by-line from index 0, report the FIRST differing pair with
// its 1-based line number; treat a length mismatch as a divergence whose
// short side reports "<eof>"; byte-identical logs return false.
inline bool find_first_divergence(const std::string&, const std::string&,
                                  uint64_t* line_no, std::string*,
                                  std::string*) {
    *line_no = 0;
    return false;  // TODO(1)
}
//@LABS-END

namespace {

struct ScratchPair {
    ScratchPair(const std::vector<std::string>& a,
                const std::vector<std::string>& b) {
        write_scratch(kScratchA, a);
        write_scratch(kScratchB, b);
    }
    ~ScratchPair() {
        std::remove(kScratchA);
        std::remove(kScratchB);
    }
};

}  // namespace

TEST(unseen, identical_logs_report_no_divergence) {
    const std::vector<std::string> lines = {"pc=0600 op=ea cyc=2",
                                            "pc=0601 op=e8 cyc=4"};
    ScratchPair s(lines, lines);
    uint64_t n = 99;
    std::string la = "x", lb = "y";
    EXPECT_FALSE(find_first_divergence(kScratchA, kScratchB, &n, &la, &lb));
    EXPECT_EQ(n, 0u);
}

TEST(unseen, locates_the_first_diverging_line) {
    const std::vector<std::string> good = {"l1", "l2", "l3=ok", "l4"};
    std::vector<std::string> bad = good;
    bad[2] = "l3=WRONG";
    ScratchPair s(good, bad);
    uint64_t n = 0;
    std::string la, lb;
    EXPECT_TRUE(find_first_divergence(kScratchA, kScratchB, &n, &la, &lb));
    EXPECT_EQ(n, 3u);                  // first divergence, not the count
    EXPECT_EQ(la, "l3=ok");
    EXPECT_EQ(lb, "l3=WRONG");
}

TEST(unseen, length_mismatch_is_a_divergence_with_eof_marker) {
    const std::vector<std::string> good = {"a", "b", "c"};
    const std::vector<std::string> bad = {"a", "b"};
    ScratchPair s(good, bad);
    uint64_t n = 0;
    std::string la, lb;
    EXPECT_TRUE(find_first_divergence(kScratchA, kScratchB, &n, &la, &lb));
    EXPECT_EQ(n, 3u);
    EXPECT_EQ(la, "c");
    EXPECT_EQ(lb, "<eof>");
}

TEST(unseen, committed_50k_pair_diverges_late_and_exactly_once) {
    if (g_good == nullptr || g_bad == nullptr) return;  // paths not supplied
    uint64_t n = 0;
    std::string la, lb;
    EXPECT_TRUE(find_first_divergence(g_good, g_bad, &n, &la, &lb));
    EXPECT_EQ(n, 49867u);              // seeded late in the pair
    EXPECT_NE(la, lb);
    // The perturbed token is the cycle count of that instruction.
    EXPECT_TRUE(la.find("cyc=") != std::string::npos);
    std::printf("divergence at line %llu:\n  good: %s\n  bad:  %s\n",
                (unsigned long long)n, la.c_str(), lb.c_str());
}

int main(int argc, char** argv) {
    // argv[1] = optional suite filter, argv[2..3] = committed fixture paths.
    const std::string filter = argc > 1 ? argv[1] : "";
    if (argc > 3) {
        g_good = argv[2];
        g_bad = argv[3];
    }
    ::labstest::run_all(filter);
    return 0;
}
