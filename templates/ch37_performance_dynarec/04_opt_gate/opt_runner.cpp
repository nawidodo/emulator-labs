// Headless runner CLI:
//   ch37_04_runner --program PATH [--max-ops N] [--check-opt]
//                  [--dump FILE] [--help]
//
// With --check-opt the runner enforces the chapter gate itself: the
// optimized pipeline must (a) reproduce the unoptimized observable dump
// byte-exactly and (b) execute at least 20% fewer IR ops. Exit 0 only when
// both hold — the deterministic, wall-time-free optimization gate.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "exec_ir.hpp"
#include "opt.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch37_04_runner --program PATH [--max-ops N] [--check-opt]\n"
        "                     [--dump FILE]\n"
        "\n"
        "Runs a program through the unoptimized and optimized IR pipelines\n"
        "and reports executed-op counts. --check-opt fails the run unless\n"
        "the optimized pipeline saves >= 20%% of executed ops while keeping\n"
        "the observable dump byte-identical.\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string program_path, dump_path;
    uint64_t max_ops = 1000000;
    bool check = false;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) return print_help();
        if (!std::strcmp(argv[i], "--program") && i + 1 < argc) {
            program_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--max-ops") && i + 1 < argc) {
            max_ops = std::strtoull(argv[++i], nullptr, 0);
        } else if (!std::strcmp(argv[i], "--check-opt")) {
            check = true;
        } else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) {
            dump_path = argv[++i];
        }
    }
    if (program_path.empty()) {
        std::fprintf(stderr, "error: --program is required (--help)\n");
        return 2;
    }

    std::ifstream in(program_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot open program '%s'\n",
                     program_path.c_str());
        return 2;
    }
    const std::vector<uint8_t> image((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());

    rx8::IrEngine base;
    base.load(image);
    base.run(max_ops);

    rx8::Machine m;
    m.load(image);
    auto blocks = rx8::translate(m, rx8::find_blocks(m, base.code_end));
    rx8::optimize(blocks);

    rx8::IrEngine opt;
    opt.load(image);
    opt.install(std::move(blocks));
    opt.run(max_ops);

    if (!dump_path.empty()) {
        std::ofstream out(dump_path, std::ios::binary);
        out << rx8::observable_dump(opt.m);
    }

    const bool dumps_match =
        rx8::observable_dump(opt.m) == rx8::observable_dump(base.m);
    const bool both_ok = base.m.halted && !base.m.fault && opt.m.halted &&
                         !opt.m.fault;
    // >= 20% reduction, integer-exact: optimized*5 <= baseline*4.
    const bool fast_enough = opt.ops_executed * 5 <= base.ops_executed * 4;

    const double reduction =
        base.ops_executed == 0
            ? 0.0
            : 100.0 * double(base.ops_executed - opt.ops_executed) /
                  double(base.ops_executed);

    std::printf("baseline=%llu optimized=%llu reduction=%.1f%% "
                "dumps_match=%d ok=%d\n",
                static_cast<unsigned long long>(base.ops_executed),
                static_cast<unsigned long long>(opt.ops_executed), reduction,
                dumps_match ? 1 : 0,
                (both_ok && dumps_match && fast_enough) ? 1 : 0);

    if (check) {
        return (both_ok && dumps_match && fast_enough) ? 0 : 1;
    }
    return both_ok ? 0 : 1;
}
