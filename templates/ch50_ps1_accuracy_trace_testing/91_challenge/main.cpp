#define LABSTEST_MAIN
#include "labstest.hpp"
#include "../01_suite_runner/checks.hpp"
#include "../01_suite_runner/suite.hpp"
#include "../shared/fnv.hpp"
#include "../shared/goldens.hpp"
#include <cstddef>

// Challenge: the whole accuracy gate in one process.
//
// The aggregate runner (ch50_01_accuracy_runner) is the CLI face of the
// suite; this test is its in-process twin. It runs the default built-in
// psx-mini suite, requires every check to pass, requires two consecutive
// runs to be byte-identical, and pins the FNV-64 of the exact report text —
// the same bytes the runner prints, so a student binary that passes ctest
// and a report that matches the golden hash are the SAME claim.
using namespace psxsuite;

namespace {

std::vector<Case> builtin_suite() {
    std::vector<Case> cases;
    for (const auto& c : kBuiltinChecks) {
        Case cs;
        cs.name = c.name;
        cs.is_builtin = true;
        cs.builtin = std::string("builtin.") + c.name;
        cs.ok = true;
        cases.push_back(std::move(cs));
    }
    return cases;
}

}  // namespace

TEST(challenge_aggregate, every_builtin_check_passes) {
    const auto results = run_suite(builtin_suite());
    EXPECT_EQ(results.size(), size_t(7));
    for (const auto& r : results) EXPECT_TRUE(r.pass);
}

TEST(challenge_aggregate, reruns_byte_identical) {
    const std::string a = format_report(run_suite(builtin_suite()));
    const std::string b = format_report(run_suite(builtin_suite()));
    EXPECT_EQ(a, b);  // determinism IS the product here
}

TEST(challenge_aggregate, report_hash_matches_golden) {
    const std::string text = format_report(run_suite(builtin_suite()));
    EXPECT_EQ(psxmini::fnv64(text), psxmini::kGoldenSuiteSummaryFnv64);
}
