// Headless runner CLI:
//   ch37_99_ext_runner --program PATH [--max-ops N] [--dump FILE] [--help]
//
// Executes a program through the full EXTENSION pipeline (translate with
// extension lowering, optimize, run) and writes the observable dump.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ext.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch37_99_ext_runner --program PATH [--max-ops N]\n"
        "                        [--dump FILE]\n"
        "\n"
        "Runs the rx8 + extensions pipeline (translate -> optimize ->\n"
        "execute) on a program image; reports executed IR ops.\n");
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

    const rx8ext::ExtResult r = rx8ext::run_ext(image, true, max_ops);

    if (!dump_path.empty()) {
        std::ofstream out(dump_path, std::ios::binary);
        out << r.dump;
    }

    std::printf("ops=%llu halted=%d fault=%d\n",
                static_cast<unsigned long long>(r.ops), r.halted ? 1 : 0,
                r.fault ? 1 : 0);
    return (r.halted && !r.fault) ? 0 : 1;
}
