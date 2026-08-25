#define LABSTEST_MAIN
#include "labstest.hpp"
#include "checks.hpp"
#include "suite.hpp"
#include <cstddef>

using namespace psxsuite;

TEST(suite_parse, builtin_case_line) {
    Case c;
    parse_case("case cpu_trace builtin.cpu_trace", c);
    EXPECT_TRUE(c.ok);
    EXPECT_EQ(c.name, std::string("cpu_trace"));
    EXPECT_TRUE(c.is_builtin);
    EXPECT_EQ(c.builtin, std::string("builtin.cpu_trace"));
}

TEST(suite_parse, binary_case_with_args) {
    Case c;
    parse_case("case seed01 build/skels/x/90_debug/ch50_90_regress_tests seed01 --verbose", c);
    EXPECT_TRUE(c.ok);
    EXPECT_FALSE(c.is_builtin);
    EXPECT_EQ(c.binary,
              std::string("build/skels/x/90_debug/ch50_90_regress_tests"));
    EXPECT_EQ(c.args.size(), size_t(2));
    // Guard: with parse_case unimplemented the vector is empty; indexing
    // it must not crash the skeleton (RED, never UB).
    if (c.args.size() == 2) {
        EXPECT_EQ(c.args[0], std::string("seed01"));
    }
}

TEST(suite_parse, rejects_malformed_lines) {
    Case a;
    parse_case("just some words", a);
    EXPECT_FALSE(a.ok);

    Case b;
    parse_case("case only_name", b);
    EXPECT_FALSE(b.ok);

    Case d;
    parse_case("case x builtin.vram extra_arg", d);
    EXPECT_FALSE(d.ok);  // builtins take no args
}

TEST(suite_parse, comments_and_blanks_skipped) {
    const auto cs = parse_suite(
        "# header comment\n"
        "\n"
        "   \n"
        "case vram builtin.vram\n"
        "  # indented comment\n"
        "case spu builtin.spu\n");
    EXPECT_EQ(cs.size(), size_t(2));
    EXPECT_TRUE(cs[0].ok);
    EXPECT_EQ(cs[0].name, std::string("vram"));
    EXPECT_EQ(cs[1].name, std::string("spu"));
}

TEST(builtin_checks, registry_complete_and_dispatching) {
    // All seven subsystems must be reachable under both spellings.
    const char* names[] = {"cpu_trace", "vram",    "spu",   "dma",
                           "gte",       "timer",   "cdrom"};
    for (const char* n : names) {
        EXPECT_NE(find_builtin(n), nullptr);
        EXPECT_NE(find_builtin(std::string("builtin.") + n), nullptr);
    }
    EXPECT_EQ(find_builtin("nope"), nullptr);
}

TEST(builtin_checks, all_builtins_pass_against_goldens) {
    std::string why;
    for (const auto& c : kBuiltinChecks) {
        std::string w;
        EXPECT_TRUE(c.fn(&w));
    }
    (void)why;
}

TEST(report, pins_summary_layout) {
    std::vector<Result> rs = {
        {"alpha", true, {}},
        {"beta", false, "exit 1: beta_bin"},
        {"gamma", true, {}},
    };
    const std::string text = format_report(rs);
    EXPECT_EQ(text,
              "PASS alpha\n"
              "FAIL beta: exit 1: beta_bin\n"
              "PASS gamma\n"
              "== 2/3 checks passed\n");
}
