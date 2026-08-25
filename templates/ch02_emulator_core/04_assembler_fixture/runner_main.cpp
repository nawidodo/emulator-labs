// ch02_04_runner — LAB-8 headless runner (curriculum §52 CLI shape).
//
//   ch02_04_runner --rom prog.bin [--cycles N] [--trace FILE] [--headless]
//
// Emits one canonical trace line per executed instruction
// (pc=<hex> op=<hex> r0..r3=<hex> cyc=<n>) and a final summary line on
// stdout. Deterministic: cycles are counted, never measured.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "cpu.hpp"

namespace {

void usage(std::FILE* out) {
    std::fprintf(out,
                 "usage: ch02_04_runner --rom PATH [--cycles N] "
                 "[--trace FILE] [--headless]\n"
                 "\n"
                 "  --rom PATH     program image, loaded at address 0x00\n"
                 "  --cycles N     cycle budget before forced stop "
                 "(default 10000)\n"
                 "  --trace FILE   write canonical trace lines to FILE\n"
                 "  --headless     accepted for CLI compatibility, no-op\n");
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* trace_path = nullptr;
    uint32_t budget = 10000;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (std::strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (std::strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
            budget = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (std::strcmp(argv[i], "--headless") == 0) {
            // no-op: kept so every chapter's runner shares one CLI shape
        } else {
            std::fprintf(stderr, "error: unknown/incomplete argument %s\n",
                         argv[i]);
            usage(stderr);
            return 1;
        }
    }

    if (!rom_path) {
        std::fprintf(stderr, "error: --rom is required\n");
        usage(stderr);
        return 1;
    }

    std::ifstream in(rom_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot open rom '%s'\n", rom_path);
        return 1;
    }
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (image.empty()) {
        std::fprintf(stderr, "error: rom '%s' is empty\n", rom_path);
        return 1;
    }

    ch02::Cpu cpu;
    cpu.load(std::span<const uint8_t>(image.data(),
                                     std::min<std::size_t>(image.size(), 256)));

    std::ofstream trace;
    if (trace_path)
        trace.open(trace_path, std::ios::binary | std::ios::trunc);

    uint32_t spent = 0;
    uint64_t steps = 0;
    while (!cpu.halted && spent < budget) {
        // Capture PRE-execution state: the trace names the instruction as it
        // was fetched plus what it cost (SPEC.md).
        const uint8_t pre_pc = cpu.pc;
        const uint8_t op = cpu.ram[pre_pc];
        const uint8_t regs[4] = {cpu.r[0], cpu.r[1], cpu.r[2], cpu.r[3]};
        const ch02::StepResult res = cpu.step();
        spent += res.cycles;
        ++steps;
        if (trace_path)
            trace << ch02::format_trace(pre_pc, op, regs, res.cycles) << '\n';
    }
    if (trace_path)
        trace.close();

    std::printf("steps=%llu cycles=%u halted=%d\n",
                static_cast<unsigned long long>(steps), spent,
                cpu.halted ? 1 : 0);
    return 0;
}
