// Headless runner CLI (curriculum §52 shape):
//   ch34_02_toy_soc_runner --program PATH --cycles N
//                          [--trace FILE] [--events FILE] [--help]
//
// Trace line format: pc=<hex> op=<hex> a=<hex> cyc=<n>  (per executed
// instruction; cyc = master-clock cycle at instruction start).
// Events file: one "cyc=<n> dev=<cpu|timer|uart>" line per dispatch.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "soc.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch34_02_toy_soc_runner --program PATH --cycles N\n"
        "                              [--trace FILE] [--events FILE]\n"
        "\n"
        "Runs the fx8 toy SoC (CPU + timer + UART) on an event-queue\n"
        "scheduler with integer guest clocks.\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string program_path, trace_path, events_path;
    uint64_t cycles = 1000;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) return print_help();
        if (!std::strcmp(argv[i], "--program") && i + 1 < argc) {
            program_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--cycles") && i + 1 < argc) {
            cycles = std::strtoull(argv[++i], nullptr, 0);
        } else if (!std::strcmp(argv[i], "--trace") && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--events") && i + 1 < argc) {
            events_path = argv[++i];
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
    std::vector<uint8_t> prog((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    // The fx8 memory is 256 bytes; reject oversized images explicitly.
    if (prog.size() > 256) {
        std::fprintf(stderr, "error: program exceeds 256 bytes\n");
        return 2;
    }

    soc::SoC soc(prog);
    soc.boot();
    // Devices keep running after the CPU halts; only the cycle budget
    // (or an empty queue) ends the run.
    while (soc.now() <= cycles) {
        if (!soc.step_event()) break;
    }

    if (!trace_path.empty()) {
        std::ofstream out(trace_path, std::ios::binary);
        for (const auto& l : soc.insn_trace) out << l << "\n";
    }
    if (!events_path.empty()) {
        std::ofstream out(events_path, std::ios::binary);
        for (const auto& l : soc.event_log) out << l << "\n";
    }

    std::printf("cycles=%llu halted=%d events=%llu uart_tx=%zu\n",
                static_cast<unsigned long long>(soc.now()),
                soc.halted() ? 1 : 0,
                static_cast<unsigned long long>(soc.events_dispatched()),
                soc.uart.transmitted.size());
    return 0;
}
