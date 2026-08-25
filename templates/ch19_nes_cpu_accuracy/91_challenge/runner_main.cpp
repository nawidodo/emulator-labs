// Headless runner for the ch19 challenge: raw 6502 programs on flat RAM
// with nestest-style logging.
//
//   ch19_91_challenge_runner --rom prog.bin [--data ADDR=HEX]...
//                           [--cycles N] [--trace-log FILE]
//
// Programs load at $0600 and start executing there; --cycles bounds the
// number of executed instructions. Memory beyond the program is
// zero-initialized unless preloaded with --data ADDR=HEX (repeatable).
#include "cpu.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct Preload {
    uint16_t addr;
    uint8_t val;
};

}  // namespace

int main(int argc, char** argv) {
    std::vector<uint8_t> program;
    std::vector<Preload> preload;
    uint64_t max_instructions = 100000;
    const char* log_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (arg == "--rom") {
            const char* path = next();
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                std::fprintf(stderr, "cannot open rom %s\n", path);
                return 2;
            }
            program.assign(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
        } else if (arg == "--cycles") {
            max_instructions = std::strtoull(next(), nullptr, 0);
        } else if (arg == "--trace-log") {
            log_path = next();
        } else if (arg == "--data") {
            unsigned addr = 0, val = 0;
            std::sscanf(next(), "%x=%x", &addr, &val);
            preload.push_back({uint16_t(addr), uint8_t(val)});
        } else if (arg == "--headless") {
            // accepted for CLI parity; this runner is always headless
        } else if (arg == "--help") {
            std::printf(
                "usage: %s --rom BIN [--data ADDR=HEX]... [--cycles N] "
                "[--trace-log FILE]\n",
                argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown flag %s\n", arg.c_str());
            return 2;
        }
    }

    nes6502::FlatRam ram;
    nes6502::Cpu cpu{.bus = &ram};
    for (const auto& pre : preload) ram.mem[pre.addr] = pre.val;
    if (!program.empty())
        cpu.load_program(0x0600, program.data(), program.size());

    int rc = 0;
    if (log_path != nullptr) {
        std::ofstream out(log_path, std::ios::binary);
        while (!cpu.halted && max_instructions > 0) {
            --max_instructions;
            const nes6502::TraceRow row = peek_trace(ram, cpu.pc);
            step(cpu);
            out << trace_line(cpu, row) << '\n';
        }
        rc = out ? 0 : 3;
    }
    return rc;
}
