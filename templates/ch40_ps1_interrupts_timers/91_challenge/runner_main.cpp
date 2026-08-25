//
// ch40 / 91_challenge — headless challenge runner
//
// Loads a raw MIPS word image at physical 0x10000 (entry 0x80010000),
// runs the deterministic machine for a fixed cycle budget and can emit an
// execution trace plus a state digest:
//
//   ch40_03_scheduler_runner --rom prog.bin --cycles 400 \
//       [--headless] [--frames N] [--trace out.log] [--hash-frame out.txt]
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>

#include "machine.hpp"

namespace {

uint64_t fnv64(const std::string& data) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (unsigned char b : data) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

std::string state_digest(const ps1::sysdev::Machine& m) {
    char buf[256];
    std::string s;
    std::snprintf(buf, sizeof(buf), "cyc=%llu istat=%04X imask=%04X\n",
                  static_cast<unsigned long long>(m.cycles),
                  m.irq.status() & 0xFFFF, m.irq.read_mask());
    s += buf;
    for (int n = 0; n < 3; ++n) {
        std::snprintf(buf, sizeof(buf), "t%d cnt=%04X mode=%04X tgt=%04X\n", n,
                      m.timers.regs[n].counter, m.timers.regs[n].mode,
                      m.timers.regs[n].target);
        s += buf;
    }
    static const uint32_t kRamWindow = 0x400;
    for (uint32_t i = 0; i < kRamWindow; i += 16) {
        std::snprintf(buf, sizeof(buf), "ram %04X:", i);
        s += buf;
        for (uint32_t j = 0; j < 16; ++j) {
            std::snprintf(buf, sizeof(buf), " %02X", m.ram[i + j]);
            s += buf;
        }
        s += '\n';
    }
    return s;
}

void usage(std::FILE* out) {
    std::fprintf(out,
                 "usage: ch40_91_challenge_runner --rom PATH [--cycles N] "
                 "[--frames N] [--headless] [--trace FILE] "
                 "[--hash-frame FILE]\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path, hash_path;
    uint64_t cycles = 1000;
    bool have_rom = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            usage(stdout);
            return 0;
        } else if (arg == "--rom" && i + 1 < argc) {
            rom_path = argv[++i];
            have_rom = true;
        } else if (arg == "--cycles" && i + 1 < argc) {
            cycles = std::strtoull(argv[++i], nullptr, 0);
        } else if (arg == "--frames" && i + 1 < argc) {
            // No GPU here: a frame is one synthetic video period.
            cycles += std::strtoull(argv[++i], nullptr, 0) *
                      ps1::sysdev::kVblankPeriod;
        } else if (arg == "--headless") {
            // accepted, no-op: this runner is always headless
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--hash-frame" && i + 1 < argc) {
            hash_path = argv[++i];
        } else {
            usage(stderr);
            return 1;
        }
    }
    if (!have_rom) {
        usage(stderr);
        return 1;
    }

    FILE* rom = std::fopen(rom_path.c_str(), "rb");
    if (!rom) {
        std::fprintf(stderr, "cannot open rom: %s\n", rom_path.c_str());
        return 1;
    }
    std::vector<uint32_t> words;
    uint8_t quad[4];
    while (std::fread(quad, 1, 4, rom) == 4) {
        words.push_back(uint32_t{quad[0]} | uint32_t{quad[1]} << 8 |
                        uint32_t{quad[2]} << 16 | uint32_t{quad[3]} << 24);
    }
    std::fclose(rom);

    ps1::sysdev::Machine m;
    std::string trace;
    if (!trace_path.empty()) m.trace_sink = &trace;
    m.load_program(words.data(), words.size());
    m.run_until(cycles);

    if (!trace_path.empty()) {
        FILE* f = std::fopen(trace_path.c_str(), "wb");
        if (!f) {
            std::fprintf(stderr, "cannot write trace: %s\n",
                         trace_path.c_str());
            return 1;
        }
        std::fwrite(trace.data(), 1, trace.size(), f);
        std::fclose(f);
    }
    if (!hash_path.empty()) {
        char payload[32];
        std::snprintf(payload, sizeof(payload), "fnv64=%016llX\n",
                      static_cast<unsigned long long>(fnv64(state_digest(m))));
        FILE* f = std::fopen(hash_path.c_str(), "wb");
        if (!f) {
            std::fprintf(stderr, "cannot write hash: %s\n",
                         hash_path.c_str());
            return 1;
        }
        std::fwrite(payload, 1, std::strlen(payload), f);
        std::fclose(f);
    }
    return 0;
}
