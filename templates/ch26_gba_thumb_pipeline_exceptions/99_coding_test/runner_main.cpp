// Headless Thumb runner for ch26 fixtures.
//
//   ch26_coding_runner --rom prog.bin --headless --cycles N --trace t.log
//                         [--dump d.txt]
//
// Trace lines follow the canonical emulator-labs format; in Thumb state `op`
// is the 16-bit halfword. CPU-only chapter: --frames behaves like --cycles.
#include <cstdio>
#include <cstring>
#include <string>
#include "coding_cpu.hpp"

int main(int argc, char** argv) {
    std::string rom_path, trace_path, dump_path;
    long long cycles = 1000;
    bool headless = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom_path = next();
        else if (a == "--headless") headless = true;
        else if (a == "--cycles" || a == "--frames") cycles = atoll(next());
        else if (a == "--trace") trace_path = next();
        else if (a == "--dump") dump_path = next();
        else if (a == "--help") {
            std::printf(
                "usage: ch26_coding_runner --rom PATH [--headless]"
                " [--cycles N] [--frames N] [--trace FILE] [--dump FILE]\n");
            return 0;
        }
    }
    if (rom_path.empty()) {
        std::fprintf(stderr, "ch26_coding_runner: --rom is required\n");
        return 2;
    }

    coding::CodingCpu cpu;
    if (FILE* f = std::fopen(rom_path.c_str(), "rb")) {
        std::fread(cpu.mem, 1, coding::CodingCpu::kMemSize, f);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "ch26_coding_runner: cannot open %s\n",
                     rom_path.c_str());
        return 2;
    }

    FILE* trace = trace_path.empty() ? nullptr : std::fopen(trace_path.c_str(), "w");
    unsigned long long total = 0;
    for (long long i = 0; i < cycles; ++i) {
        const uint32_t pc = cpu.r[15];
        const unsigned op = cpu.read16(pc);
        const unsigned c = cpu.step();
        total += c;
        if (trace) {
            std::fprintf(trace, "pc=%08x op=%04x cyc=%llu", pc, op, total);
            for (int rg = 0; rg < 16; ++rg)
                std::fprintf(trace, " r%d=%08x", rg, cpu.r[rg]);
            std::fprintf(trace, " cpsr=%08x\n", cpu.cpsr);
        }
    }
    if (trace) std::fclose(trace);

    if (!dump_path.empty()) {
        if (FILE* d = std::fopen(dump_path.c_str(), "w")) {
            for (int rg = 0; rg < 16; ++rg)
                std::fprintf(d, "r%d=%08x\n", rg, cpu.r[rg]);
            std::fprintf(d, "cpsr=%08x\ncycles=%llu\n", cpu.cpsr, total);
            std::fclose(d);
        }
    }
    return 0;
}
