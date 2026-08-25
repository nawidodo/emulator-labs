// Headless runner CLI (curriculum §52 shape, adapted for a CPU-only core):
//   ch37_01_runner --program PATH [--max-steps N] [--trace FILE]
//                  [--dump FILE] [--help]
//
// Prints `instructions=<n>` — the executed-instruction count that every
// later exercise in this chapter optimizes. Deterministic by construction:
// no wall time anywhere.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "rx8.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch37_01_runner --program PATH [--max-steps N]\n"
        "                     [--trace FILE] [--dump FILE]\n"
        "\n"
        "Runs the rx8 switch-dispatch interpreter on a program image and\n"
        "reports the retired-instruction count (the performance proxy).\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string program_path, trace_path, dump_path;
    uint64_t max_steps = 100000;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) return print_help();
        if (!std::strcmp(argv[i], "--program") && i + 1 < argc) {
            program_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--max-steps") && i + 1 < argc) {
            max_steps = std::strtoull(argv[++i], nullptr, 0);
        } else if (!std::strcmp(argv[i], "--trace") && i + 1 < argc) {
            trace_path = argv[++i];
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

    rx8::Machine m;
    m.load(image);

    std::vector<std::string> trace;
    uint64_t cyc = 0;
    while (cyc < max_steps) {
        if (m.halted || m.fault) break;
        const uint32_t at = m.pc;
        const uint8_t op = uint8_t(m.read_le(at) >> 24);
        ++cyc;
        if (m.step() == 0) break;  // fetch fault
        trace.push_back(rx8::trace_line(m, at, op, cyc));
    }

    if (!trace_path.empty()) {
        std::ofstream out(trace_path, std::ios::binary);
        for (const auto& l : trace) out << l << "\n";
    }
    if (!dump_path.empty()) {
        std::ofstream out(dump_path, std::ios::binary);
        out << rx8::observable_dump(m);
    }

    std::printf("instructions=%llu halted=%d fault=%d\n",
                static_cast<unsigned long long>(m.executed),
                m.halted ? 1 : 0, m.fault ? 1 : 0);
    return (m.halted && !m.fault) ? 0 : 1;
}
