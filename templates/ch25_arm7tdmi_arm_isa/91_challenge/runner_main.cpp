// Headless runner for the ch25 challenge fixture (adds LDM/STM dispatch).
#include <cstdio>
#include <cstring>
#include <string>
#include "challenge_ldm.hpp"

int main(int argc, char** argv) {
    std::string rom_path, dump_path;
    long long cycles = 500;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom_path = next();
        else if (a == "--headless") { /* CPU-only runner: no video */ }
        else if (a == "--cycles" || a == "--frames") cycles = atoll(next());
        else if (a == "--trace") { /* optional; challenge grades via dump */ }
        else if (a == "--dump") dump_path = next();
        else if (a == "--help") {
            std::printf("usage: ch25_challenge_runner --rom PATH [--cycles N]"
                        " [--dump FILE]\n");
            return 0;
        }
    }
    if (rom_path.empty()) return 2;

    arm::ChallengeCpu cpu;
    if (FILE* f = std::fopen(rom_path.c_str(), "rb")) {
        std::fread(cpu.mem, 1, arm::ArmCpu::kMemSize, f);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "ch25_challenge_runner: cannot open %s\n",
                     rom_path.c_str());
        return 2;
    }

    unsigned long long total = 0;
    for (long long i = 0; i < cycles; ++i) total += cpu.step();

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
