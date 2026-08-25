// ch38 challenge runner — headless R3000A interpreter over a synthetic
// program fixture (docs/AUTHORING.md runner CLI).
//
//   ch38_91_challenge_runner --rom prog.bin [--cycles N] [--frames N]
//                            [--headless] [--trace FILE] [--hash-frame FILE]
//
// The program loads at 0x80010000 and starts executing there. Execution
// stops on the cycle budget or on syscall/break/unknown-opcode halt.
// --hash-frame writes "fnv64=<hex>\n" over the full 2 MB RAM image, which is
// deterministic for a deterministic program.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "interp.hpp"
#include "trace.hpp"

namespace {

void print_usage() {
    std::fputs("usage: ch38_91_challenge_runner --rom PATH [--cycles N] "
               "[--frames N] [--headless] [--trace FILE] [--hash-frame FILE]\n",
               stderr);
}

[[noreturn]] void usage_fail() {
    print_usage();
    std::exit(2);
}

uint64_t fnv1a64(const uint8_t* data, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path, hash_path;
    long long cycles = 10000;
    long long frames = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--rom") rom_path = need_value("--rom");
        else if (a == "--trace") trace_path = need_value("--trace");
        else if (a == "--hash-frame") hash_path = need_value("--hash-frame");
        else if (a == "--cycles") cycles = std::atoll(need_value("--cycles").c_str());
        else if (a == "--frames") frames = std::atoll(need_value("--frames").c_str());
        else if (a == "--headless") { /* accepted: CPU-only runner is headless */ }
        else if (a == "--help") { print_usage(); return 0; }
        else usage_fail();
    }
    if (rom_path.empty()) usage_fail();
    // One "frame" is modeled as 2000 steps: plenty for CPU-only fixtures.
    if (frames >= 0 && frames * 2000 > cycles) cycles = frames * 2000;

    std::ifstream rom(rom_path, std::ios::binary);
    if (!rom) {
        std::fprintf(stderr, "cannot open rom '%s'\n", rom_path.c_str());
        return 1;
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(rom)),
                                     std::istreambuf_iterator<char>());

    psx::r3000a::Bus bus;
    psx::r3000a::CpuState cpu;
    cpu.load_program(bus, bytes.data(), bytes.size());

    std::ofstream trace;
    if (!trace_path.empty()) trace.open(trace_path);

    while (!cpu.halted && cpu.cycles < static_cast<uint64_t>(cycles)) {
        const uint32_t pc = cpu.window.current_pc;
        const uint32_t instr = bus.read32(pc);
        const psx::r3000a::StepResult r = psx::r3000a::cpu_step(cpu, bus);
        if (trace) {
            trace << psx::r3000a::format_trace_line(pc, instr, r.cycles) << "\n";
        }
    }

    if (!hash_path.empty()) {
        const uint64_t h = fnv1a64(bus.ram, sizeof(bus.ram));
        std::ofstream out(hash_path);
        char buf[32];
        std::snprintf(buf, sizeof buf, "fnv64=%016llX\n",
                      static_cast<unsigned long long>(h));
        out << buf;
    }

    std::printf("halted=%d cycles=%llu pc=%08X\n", cpu.halted ? 1 : 0,
                static_cast<unsigned long long>(cpu.cycles),
                cpu.window.current_pc);
    return 0;
}
