// Headless runner for the coding test: the full Chapter 11 machine (91
// challenge machine) with the ldh hook installed, so hidden .bin fixtures
// exercise the $FF00-page family end-to-end.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../01_daa_rotates/core.hpp"
#include "../91_challenge/machine.hpp"
#include "ldh.hpp"

namespace {

struct Options {
    const char* rom = nullptr;
    long cycles = 1000000;
    const char* trace = nullptr;
    const char* hash_frame = nullptr;
};

std::vector<uint8_t> read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(2);
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0) std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    return data;
}

void write_file(const char* path, const std::string& text) {
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", path);
        std::exit(2);
    }
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--rom") opt.rom = next();
        else if (a == "--cycles") opt.cycles = std::atol(next());
        else if (a == "--trace") opt.trace = next();
        else if (a == "--hash-frame") opt.hash_frame = next();
        else if (a == "--headless" || a == "--frames" || a == "--input-file") {
            if (a == "--frames" || a == "--input-file") (void)next();
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: %s --rom PATH [--cycles N] [--trace FILE] "
                "[--hash-frame FILE] --headless\n",
                argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return 2;
        }
    }
    if (!opt.rom) {
        std::fprintf(stderr, "missing --rom\n");
        return 2;
    }

    gb::Machine m;
    gb::install_ldh_hook(m.cpu);  // first in the chain: claims E0/F0/E2/F2/08
    m.load(read_file(opt.rom));

    std::string trace;
    // Step cap mirrors the cycle budget: keeps the skeleton build bounded
    // (its step() stub never advances cyc).
    long steps = 0;
    while (m.cpu.cyc < static_cast<uint64_t>(opt.cycles) && steps < 10000000) {
        if (m.cpu.trap) break;
        if (m.cpu.halted && m.ctl.pending() == 0) break;
        const uint16_t instr_pc = m.cpu.pc;
        const uint8_t op = m.bus.read(instr_pc);
        m.step();
        ++steps;
        trace += gb::trace_line(instr_pc, op, m.cpu);
    }

    if (opt.trace) write_file(opt.trace, trace);
    if (opt.hash_frame) write_file(opt.hash_frame, gb::state_dump(m.cpu));

    if (m.cpu.trap) {
        std::fprintf(stderr,
                     "TRAP: unimplemented opcode %02X at %04X after %llu cycles\n",
                     m.bus.read(m.cpu.pc), m.cpu.pc,
                     static_cast<unsigned long long>(m.cpu.cyc));
        return 1;
    }
    // One-line summary on stdout: the canonical final-state dump.
    std::printf("%s", gb::state_dump(m.cpu).c_str());
    return 0;
}
