// Headless 8080 runner — the canonical CLI shape used by every chapter
// (AUTHORING.md "Runner CLI"). Grading drives this binary directly.
//
//   i8080_runner --rom prog.bin --cycles 500 [--trace t.log]
//                [--dump-state final.txt] [--headless] [--input-file f]
//
// The ROM loads at 0x0000 and execution starts at PC=0. Execution stops at
// HLT, after --cycles T-states, or when PC wraps past 64 KiB.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.hpp"

namespace {

struct Options {
    std::string rom;
    std::string trace;
    std::string dump_state;
    std::string input_file;    // accepted for CLI parity; unused on 8080 core
    uint64_t max_cycles = 10000;
    bool headless = true;
};

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : "";
        };
        if (a == "--rom") o.rom = next();
        else if (a == "--trace") o.trace = next();
        else if (a == "--dump-state") o.dump_state = next();
        else if (a == "--input-file") o.input_file = next();
        else if (a == "--cycles") o.max_cycles = strtoull(next().c_str(), nullptr, 0);
        else if (a == "--frames") (void)next();       // parity flag, ignored
        else if (a == "--hash-frame") (void)next();   // parity flag, ignored
        else if (a == "--headless") o.headless = true;
        else { /* unknown flags ignored for forward compatibility */ }
    }
    return o;
}

std::string state_line(const i8080::Cpu& cpu) {
    char buf[128];
    const uint8_t f =
        i8080::pack_psw(uint8_t((cpu.s ? i8080::FLAG_S : 0) |
                                (cpu.z ? i8080::FLAG_Z : 0) |
                                (cpu.ac ? i8080::FLAG_AC : 0) |
                                (cpu.p ? i8080::FLAG_P : 0) |
                                (cpu.cy ? i8080::FLAG_CY : 0)));
    snprintf(buf, sizeof buf,
             "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X "
             "PC=%04X cyc=%llu",
             cpu.a, f, cpu.b, cpu.c, cpu.d, cpu.e, cpu.h, cpu.l, cpu.sp,
             cpu.pc, (unsigned long long)cpu.cycles);
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        std::cout <<
            "usage: i8080_runner --rom PATH [--cycles N] [--trace FILE]\n"
            "                   [--dump-state FILE] [--headless]\n"
            "                   [--input-file FILE]\n";
        return 0;
    }

    const Options opts = parse_args(argc, argv);
    if (opts.rom.empty()) {
        std::cerr << "error: --rom is required\n";
        return 2;
    }

    std::ifstream rom(opts.rom, std::ios::binary);
    if (!rom) {
        std::cerr << "error: cannot open rom '" << opts.rom << "'\n";
        return 2;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(rom)),
                               std::istreambuf_iterator<char>());

    i8080::FlatBus bus;
    for (size_t i = 0; i < bytes.size(); ++i)
        bus.mem[i] = bytes[i];

    i8080::Cpu cpu;
    cpu.bus = &bus;

    std::ofstream trace_out;
    if (!opts.trace.empty()) {
        trace_out.open(opts.trace, std::ios::binary);
        if (!trace_out) {
            std::cerr << "error: cannot write trace '" << opts.trace << "'\n";
            return 2;
        }
    }

    while (!cpu.halted && cpu.cycles < opts.max_cycles) {
        if (trace_out.is_open()) {
            char line[96];
            // Trace BEFORE execution: pc/op of the instruction about to run,
            // register state, cumulative cycle count.
            snprintf(line, sizeof line,
                     "pc=%04X op=%02X af=%02X%02X bc=%02X%02X de=%02X%02X "
                     "hl=%02X%02X sp=%04X cyc=%llu\n",
                     cpu.pc, bus.mem[cpu.pc], cpu.a,
                     uint8_t((cpu.s ? 0x80 : 0) | (cpu.z ? 0x40 : 0) |
                             (cpu.ac ? 0x10 : 0) | (cpu.p ? 0x04 : 0) |
                             (cpu.cy ? 0x01 : 0) | 0x02),
                     cpu.b, cpu.c, cpu.d, cpu.e, cpu.h, cpu.l, cpu.sp,
                     (unsigned long long)cpu.cycles);
            trace_out << line;
        }
        cpu.step();
    }

    const std::string final_state = state_line(cpu);
    std::cout << final_state << "\n";
    if (!opts.dump_state.empty()) {
        std::ofstream out(opts.dump_state, std::ios::binary);
        out << final_state << "\n";
    }
    return 0;
}
