// Deterministic timing report for the ch27 challenge golden.
#include <cstdio>
#include "timing_calc.hpp"

using namespace gba;

int main(int argc, char** argv) {
    // Optional output file (hidden grading hashes the file contents).
    FILE* out = argc > 1 ? std::fopen(argv[1], "w") : stdout;
    if (!out) return 2;
    struct Row { const char* name; Region r; unsigned count, width; };
    const Row rows[] = {
        {"WS0 x16 burst8 ", Region::RomWs0, 8, 2},
        {"WS1 x16 burst8 ", Region::RomWs1, 8, 2},
        {"WS2 x16 burst8 ", Region::RomWs2, 8, 2},
        {"EWRAM w32 burst4", Region::Ewram, 4, 4},
        {"IWRAM x16 burst16", Region::Iwram, 16, 2},
        {"SRAM x8  burst4 ", Region::Sram, 4, 1},
    };
    for (const auto& row : rows)
        std::fprintf(out, "%s = %llu\n", row.name,
                    static_cast<unsigned long long>(
                        burst_total(row.r, 0x08000000u, row.count,
                                    row.width)));
    std::fprintf(out, "fastest burst1 = %d\n",
                static_cast<int>(fastest_rom_chip(1, 2)));
    std::fprintf(out, "fastest burst16 = %d\n",
                static_cast<int>(fastest_rom_chip(16, 2)));
    std::fclose(out == stdout ? nullptr : out);
    return 0;
}
