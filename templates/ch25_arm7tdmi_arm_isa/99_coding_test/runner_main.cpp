// Headless runner for the ch25 coding-test fixtures (status/swap family).
#include <cstdio>
#include <cstring>
#include <string>
#include "coding_cpu.hpp"

int main(int argc, char** argv) {
    std::string rom_path, trace_path, dump_path;
    long long cycles = 500;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom_path = next();
        else if (a == "--headless") { /* CPU-only */ }
        else if (a == "--cycles" || a == "--frames") cycles = atoll(next());
        else if (a == "--trace") trace_path = next();
        else if (a == "--dump") dump_path = next();
        else if (a == "--help") {
            std::printf("usage: ch25_coding_runner --rom PATH [--cycles N]"
                        " [--trace FILE] [--dump FILE]\n");
            return 0;
        }
    }
    if (rom_path.empty()) return 2;

    arm::CodingTestCpu cpu;
    if (FILE* f = std::fopen(rom_path.c_str(), "rb")) {
        std::fread(cpu.mem, 1, arm::ArmCpu::kMemSize, f);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "ch25_coding_runner: cannot open %s\n",
                     rom_path.c_str());
        return 2;
    }

    FILE* trace =
        trace_path.empty() ? nullptr : std::fopen(trace_path.c_str(), "w");
    unsigned long long total = 0;
    for (long long i = 0; i < cycles; ++i) {
        const uint32_t pc = cpu.r[15];
        const uint32_t op = cpu.read32(pc);
        const unsigned c = cpu.step();
        total += c;
        if (trace) {
            std::fprintf(trace, "pc=%08x op=%08x cyc=%llu", pc, op, total);
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
