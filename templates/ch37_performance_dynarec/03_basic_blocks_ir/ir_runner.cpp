// Headless runner CLI:
//   ch37_03_runner --program PATH [--max-ops N] [--dump FILE] [--help]
//
// Runs a program through the blocks -> IR pipeline (lazy per-block
// translation with store-driven flushing) and reports executed IR ops.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "exec_ir.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch37_03_runner --program PATH [--max-ops N]\n"
        "                     [--dump FILE]\n"
        "\n"
        "Runs the rx8 program through basic-block analysis + the tiny IR\n"
        "pipeline and reports executed IR operations.\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string program_path, dump_path;
    uint64_t max_ops = 1000000;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) return print_help();
        if (!std::strcmp(argv[i], "--program") && i + 1 < argc) {
            program_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--max-ops") && i + 1 < argc) {
            max_ops = std::strtoull(argv[++i], nullptr, 0);
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

    rx8::IrEngine eng;
    eng.load(image);
    eng.run(max_ops);

    if (!dump_path.empty()) {
        std::ofstream out(dump_path, std::ios::binary);
        out << rx8::observable_dump(eng.m);
    }

    std::printf("ops=%llu halted=%d fault=%d flushes=%llu\n",
                static_cast<unsigned long long>(eng.ops_executed),
                eng.m.halted ? 1 : 0, eng.m.fault ? 1 : 0,
                static_cast<unsigned long long>(eng.flushes));
    return (eng.m.halted && !eng.m.fault) ? 0 : 1;
}
