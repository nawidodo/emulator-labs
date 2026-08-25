// Headless NROM boot runner (ch20 challenge).
//
//   ch20_91_boot_runner --rom game.nes [--headless] [--cycles N]
//                       [--trace FILE] [--dump-ram FILE]
//                       [--expect-ram ADDR=HEX]...
//
// Loads the iNES image (mapper 0), resets through the cartridge vector and
// runs until halt or the cycle budget expires. --dump-ram writes the 2KB
// internal RAM; --expect-ram checks cells and exits 1 on any mismatch.
#include "nesboot.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<uint8_t> rom;
    uint64_t max_cycles = 100000;
    const char* trace_path = nullptr;
    const char* dump_path = nullptr;
    std::vector<std::pair<unsigned, unsigned>> expect;

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
            rom.assign(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
        } else if (arg == "--cycles") {
            max_cycles = std::strtoull(next(), nullptr, 0);
        } else if (arg == "--trace") {
            trace_path = next();
        } else if (arg == "--dump-ram") {
            dump_path = next();
        } else if (arg == "--expect-ram") {
            unsigned addr = 0, val = 0;
            std::sscanf(next(), "%x=%x", &addr, &val);
            expect.emplace_back(addr, val);
        } else if (arg == "--headless") {
            // accepted for CLI parity; this runner is always headless
        } else if (arg == "--help") {
            std::printf(
                "usage: %s --rom NES [--headless] [--cycles N] "
                "[--trace FILE] [--dump-ram FILE] "
                "[--expect-ram ADDR=HEX]...\n",
                argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown flag %s\n", arg.c_str());
            return 2;
        }
    }

    nesboot::Bus bus;
    nesboot::Cpu cpu{.bus = &bus};
    if (!nesboot::load_ines(bus, rom)) {
        std::fprintf(stderr, "unsupported or malformed iNES image\n");
        return 3;
    }

    std::vector<std::string> lines;
    nesboot::reset(cpu);
    while (!cpu.halted && cpu.cycles < max_cycles) {
        const uint16_t pc_before = cpu.pc;
        const int billed = nesboot::step(cpu);
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "pc=%04x a=%02x x=%02x y=%02x sp=%02x cyc=%llu n=%d",
                      pc_before, cpu.a, cpu.x, cpu.y, cpu.sp,
                      (unsigned long long)cpu.cycles, billed);
        lines.emplace_back(buf);
    }

    int rc = 0;
    if (trace_path != nullptr) {
        std::ofstream out(trace_path, std::ios::binary);
        for (const auto& l : lines) out << l << '\n';
        rc = out ? rc : 4;
    }
    if (dump_path != nullptr) {
        std::ofstream out(dump_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bus.ram.data()), bus.ram.size());
        rc = out ? rc : 4;
    }
    for (const auto& [addr, val] : expect) {
        const uint8_t got = bus.ram[addr & 0x07FF];
        if (got != val) {
            std::fprintf(stderr, "RAM mismatch at %04X: got %02X want %02X\n",
                         addr, got, val);
            rc = 1;
        }
    }
    return rc;
}
