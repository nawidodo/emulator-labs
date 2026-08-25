// Raster-timing snapshot runner (course-original fixture grammar):
//
//   ch22_91_timing_runner --script FILE [--trace OUT] [--hash-frame OUT]
//
// Script grammar (one instruction per line, '#' comments):
//   ctrl <val>            set rendering enable + PPUCTRL state
//   scroll <f> <c>        fine X, coarse X via first $2005 half
//   write <line> <dot> <reg> <val>   PPU register write ($2000-$2007)
//   snap
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "timing.hpp"

int main(int argc, char** argv) {
    std::string script_path, trace_path, hash_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf(
                "usage: ch22_91_timing_runner --script FILE [flags]\n"
                "  --script PATH      timing script (grammar in header)\n"
                "  --trace FILE       snapshot lines out\n"
                "  --hash-frame FILE  FNV64 of the snapshot stream\n");
            return 0;
        } else if (a == "--script") script_path = next();
        else if (a == "--trace") trace_path = next();
        else if (a == "--hash-frame") hash_path = next();
        else { std::fprintf(stderr, "unknown flag %s\n", a.c_str()); return 2; }
    }

    FILE* sf = fopen(script_path.c_str(), "r");
    if (!sf) {
        std::fprintf(stderr, "cannot open script %s\n", script_path.c_str());
        return 2;
    }

    nes22timing::PpuTiming p;
    std::string snaps;
    char line[256];
    while (fgets(line, sizeof(line), sf)) {
        char op[16];
        int a1, a2, a3, a4;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%15s", op) != 1) continue;
        if (strcmp(op, "ctrl") == 0 && sscanf(line, "%*s %d", &a1) == 1) {
            // bit3 = background enable for this model
            p.rendering = (a1 & 0x08) != 0;
        } else if (strcmp(op, "scroll") == 0 &&
                   sscanf(line, "%*s %d %d", &a1, &a2) == 2) {
            // direct latch load: fine X + coarse X into t/x (test hook)
            p.l.x = uint8_t(a1);
            p.l.t = uint16_t((p.l.t & ~0x001Fu) | (a2 & 0x1F));
        } else if (strcmp(op, "write") == 0 &&
                   sscanf(line, "%*s %d %d %x %x", &a1, &a2, &a3, &a4) == 4) {
            nes22timing::run_to(p, a1, a2);
            switch (a3) {
                case 0x2000: nes22scroll::ctrl_write(p.l, uint8_t(a4)); break;
                case 0x2005: nes22scroll::scroll_write(p.l, uint8_t(a4)); break;
                case 0x2006: nes22scroll::addr_write(p.l, uint8_t(a4)); break;
                case 0x2002: nes22scroll::status_read(p.l); break;
                default: break;  // $2007 side effects not needed here
            }
        } else if (strcmp(op, "runto") == 0 &&
                   sscanf(line, "%*s %d %d", &a1, &a2) == 2) {
            nes22timing::run_to(p, a1, a2);
        } else if (strcmp(op, "snap") == 0) {
            snaps += nes22timing::snapshot_text(p) + "\n";
        }
    }
    fclose(sf);

    uint64_t h = 0xCBF29CE484222325ULL;
    for (char c : snaps) {
        h ^= uint8_t(c);
        h *= 0x100000001B3ULL;
    }

    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        fwrite(snaps.data(), 1, snaps.size(), tf);
        fclose(tf);
    }
    std::printf("fnv64=%016llX\nsnaps=%zu\n",
                (unsigned long long)h, size_t(std::count(snaps.begin(),
                                                snaps.end(), '\n')));
    return 0;
}
