// Headless DMA/timer/IRQ runner (curriculum §52 CLI shape).
//
//   ch29_hw_runner --rom script.hws --cycles N [--trace F] [--hash-frame F]
//
// .hws script format: repeating little-endian records
// {u32 cycle, u32 addr, u16 value, u16 pad}; terminator cycle == 0xFFFFFFFF.
// Writes land in the bus at exactly that guest cycle.
#include <cstdio>
#include <cstring>
#include <string>
#include "system.hpp"
#include <cstddef>
using namespace gba;

namespace {

void usage() {
    std::printf(
        "ch29_hw_runner — headless GBA DMA/timer/IRQ scheduler\n"
        "usage: ch29_hw_runner --rom FILE --cycles N [options]\n"
        "  --rom PATH         .hws scripted register writes (required)\n"
        "  --cycles N         simulate N guest cycles\n"
        "  --frames N         alternative to --cycles (N * 228 * 1232)\n"
        "  --headless         accepted for CLI compatibility (no-op)\n"
        "  --input-file FILE  accepted for CLI compatibility (ignored)\n"
        "  --trace FILE       write event trace lines\n"
        "  --hash-frame FILE  write FNV-64 of final machine state\n");
}

bool load_script(const char* path, gba::HWSystem& sys) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    while (true) {
        u8 rec[12];
        if (std::fread(rec, 1, 12, f) != 12) {
            std::fclose(f);
            return false;
        }
        u32 cyc = u32(rec[0]) | u32(rec[1]) << 8 | u32(rec[2]) << 16 |
                  u32(rec[3]) << 24;
        if (cyc == 0xFFFFFFFFu) break;
        u32 addr = u32(rec[4]) | u32(rec[5]) << 8 | u32(rec[6]) << 16 |
                   u32(rec[7]) << 24;
        u16 val = u16(rec[8] | rec[9] << 8);
        sys.schedule_script_write(cyc, addr, val);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom = nullptr;
    const char* trace = nullptr;
    const char* hash = nullptr;
    gba::u64 cycles = 100000;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char** out) {
            if (i + 1 >= argc) return false;
            *out = argv[++i];
            return true;
        };
        if (a == "--help") {
            usage();
            return 0;
        } else if (a == "--headless" || a == "--input-file") {
            const char* ign = nullptr;
            if (a == "--input-file") next(&ign);  // accepted, no input model
        } else if (a == "--rom") {
            if (!next(&rom)) return 2;
        } else if (a == "--trace") {
            if (!next(&trace)) return 2;
        } else if (a == "--hash-frame") {
            if (!next(&hash)) return 2;
        } else if (a == "--cycles") {
            if (i + 1 >= argc) return 2;
            cycles = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--frames") {
            if (i + 1 >= argc) return 2;
            cycles = std::strtoull(argv[++i], nullptr, 10) * 228ull * 1232ull;
        } else {
            usage();
            return 2;
        }
    }
    if (!rom) {
        usage();
        return 2;
    }

    gba::HWSystem sys;
    if (!load_script(rom, sys)) {
        std::fprintf(stderr, "error: cannot load script %s\n", rom);
        return 1;
    }
    sys.schedule_video();
    for (int n = 0; n < 4; ++n) sys.schedule_timer(n);
    sys.sched.run_until(cycles);

    if (trace) {
        FILE* f = std::fopen(trace, "w");
        if (!f) return 1;
        for (const auto& line : sys.trace)
            std::fprintf(f, "pc=00000000 %s\n", line.c_str());
        std::fclose(f);
    }
    if (hash) {
        // Digest over IO shadow, timer counters and pending IRQ flags.
        u64 h = 0xCBF29CE484222325ull;
        auto mix = [&](const void* p, size_t n) {
            const u8* b = static_cast<const u8*>(p);
            for (size_t i = 0; i < n; ++i) {
                h ^= b[i];
                h *= 0x100000001B3ull;
            }
        };
        mix(sys.bus.io, sizeof(sys.bus.io));
        for (int n = 0; n < 4; ++n) mix(&sys.tm[n], sizeof(gba::Timer));
        mix(&sys.irq.iff, sizeof(sys.irq.iff));
        FILE* f = std::fopen(hash, "w");
        if (!f) return 1;
        std::fprintf(f, "%016llX\n", (unsigned long long)h);
        std::fclose(f);
    }
    std::printf("simulated %llu cycles, %zu events\n",
                (unsigned long long)cycles, sys.trace.size());
    return 0;
}
