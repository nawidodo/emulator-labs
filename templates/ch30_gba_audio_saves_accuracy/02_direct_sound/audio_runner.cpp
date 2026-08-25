// Headless audio runner: renders N PCM samples from a scripted FIFO state.
// Script (.hws format): {u32 cycle, u32 addr, u16 value} writes where FIFO
// addresses 0x040000A0/A4 push bytes into A/B. Deterministic output hashed
// with FNV-64.
#include <cstdio>
#include <cstring>
#include <string>
#include "dsound.hpp"

using namespace gba;

namespace {
void usage() {
    std::printf(
        "ch30_audio_runner — headless GBA Direct Sound renderer\n"
        "usage: ch30_audio_runner --rom FILE [--samples N] [--headless]\n"
        "       [--trace FILE] [--hash-frame FILE] [--input-file FILE]\n"
        "  FIFO A data via u16 writes at 0x040000A0, B at 0x040000A4\n");
}
bool load_script(const char* path, SoundFifo& a, SoundFifo& b) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    while (true) {
        u8 rec[12];
        if (std::fread(rec, 1, 12, f) != 12) break;
        u32 cyc = u32(rec[0]) | u32(rec[1]) << 8 | u32(rec[2]) << 16 |
                  u32(rec[3]) << 24;
        if (cyc == 0xFFFFFFFFu) break;
        u32 addr = u32(rec[4]) | u32(rec[5]) << 8 | u32(rec[6]) << 16 |
                   u32(rec[7]) << 24;
        u16 v = u16(rec[8] | rec[9] << 8);
        if (addr == 0x040000A0u || addr == 0x040000A4u) {
            SoundFifo& fifo = addr == 0x040000A0u ? a : b;
            fifo.push(u8(v));
            fifo.push(u8(v >> 8));
        }
    }
    std::fclose(f);
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    const char* rom = nullptr;
    const char* trace = nullptr;
    const char* hash = nullptr;
    int samples = 128;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char** o) {
            if (i + 1 >= argc) return false;
            *o = argv[++i];
            return true;
        };
        const char* p = nullptr;
        if (arg == "--help") {
            usage();
            return 0;
        } else if (arg == "--rom") {
            if (!next(&rom)) return 2;
        } else if (arg == "--trace") {
            if (!next(&trace)) return 2;
        } else if (arg == "--hash-frame") {
            if (!next(&hash)) return 2;
        } else if (arg == "--samples") {
            if (i + 1 >= argc) return 2;
            samples = std::atoi(argv[++i]);
        }
        // --headless / --input-file accepted and ignored (CLI parity).
        (void)p;
    }
    if (!rom) {
        usage();
        return 2;
    }
    SoundFifo a, b;
    a.reset();
    b.reset();
    if (!load_script(rom, a, b)) {
        std::fprintf(stderr, "error: cannot load %s\n", rom);
        return 1;
    }
    std::vector<u16> pcm;
    u64 h = render_pcm(a, b, 0, 2, 2, 512, 64, samples, pcm);
    if (trace) {
        FILE* f = std::fopen(trace, "w");
        for (size_t i = 0; i < pcm.size(); ++i)
            std::fprintf(f, "pc=00000000 op=pcm n=%zu val=%04X cyc=%llu\n", i,
                         pcm[i], (unsigned long long)(i * 64));
        std::fclose(f);
    }
    if (hash) {
        FILE* f = std::fopen(hash, "w");
        std::fprintf(f, "%016llX\n", (unsigned long long)h);
        std::fclose(f);
    }
    std::printf("rendered %d samples\n", samples);
    return 0;
}
