// ch50_01_accuracy_runner — the chapter's aggregate accuracy-suite runner.
//
//   ch50_01_accuracy_runner                run the built-in psx-mini suite
//   ch50_01_accuracy_runner SUITE.txt      run a manifest (see suite.hpp)
//   ch50_01_accuracy_runner --list         list built-in checks
//
// Prints one PASS/FAIL line per case plus "== p/t checks passed" and exits
// 0 iff every case passed. This is the same shape `ctest -L accuracy` and
// the hidden grader consume; see README.md for how they map together.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "checks.hpp"
#include "suite.hpp"

namespace {

std::vector<psxsuite::Case> default_builtin_suite() {
    std::vector<psxsuite::Case> cases;
    for (const auto& c : psxsuite::kBuiltinChecks) {
        psxsuite::Case cs;
        cs.name = c.name;
        cs.is_builtin = true;
        cs.builtin = std::string("builtin.") + c.name;
        cs.ok = true;
        cases.push_back(std::move(cs));
    }
    return cases;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& c : psxsuite::kBuiltinChecks)
            std::printf("builtin.%-10s %s\n", c.name, c.what);
        return 0;
    }

    std::vector<psxsuite::Case> cases;
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        std::fprintf(stderr,
                     "usage: %s [SUITE_FILE] | --list\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        FILE* f = std::fopen(argv[1], "r");
        if (!f) {
            std::fprintf(stderr, "cannot open suite file: %s\n", argv[1]);
            return 2;
        }
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
        cases = psxsuite::parse_suite(text);
    } else {
        cases = default_builtin_suite();
    }

    // Exit 0 iff every case passed; an empty or fully-malformed suite is
    // never a pass.
    const std::vector<psxsuite::Result> results = psxsuite::run_suite(cases);
    std::fputs(psxsuite::format_report(results).c_str(), stdout);
    for (const auto& r : results)
        if (!r.pass) return 1;
    return results.empty() ? 1 : 0;
}
