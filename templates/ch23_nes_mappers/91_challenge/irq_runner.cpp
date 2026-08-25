// MMC3 IRQ-behavior lab runner (course-original fixture grammar):
//
//   ch23_91_irq_runner --script FILE [--trace OUT] [--hash-frame OUT]
//
// Script grammar (one op per line, '#' comments):
//   wr <hexaddr> <hexval>   CPU write into the mapper's $8000-$FFFF window
//   edge <n>                fire <n> PPU A12 rising edges (default 1)
//   snap                    log "edge=<t> cnt=<c> latch=<l> en=<e> irq=<b>"
//
// Whenever the IRQ line rises (0 -> 1) the runner also logs "IRQ@<t>"
// where <t> is the total number of edges fired so far — that is exactly
// the golden interrupt log the challenge asks you to reproduce.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "mapper.hpp"
#include "mmc3.hpp"

using nes23map::Cart;

namespace {
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;
}  // namespace

int main(int argc, char** argv) {
    std::string script_path, trace_path, hash_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf(
                "usage: ch23_91_irq_runner --script FILE [flags]\n"
                "  --script PATH      IRQ lab script (grammar in header)\n"
                "  --trace FILE       interrupt log out\n"
                "  --hash-frame FILE  FNV64 of the log stream\n");
            return 0;
        } else if (a == "--script") script_path = next();
        else if (a == "--trace") trace_path = next();
        else if (a == "--hash-frame") hash_path = next();
        else { std::fprintf(stderr, "unknown flag %s\n", a.c_str()); return 2; }
    }
    if (script_path.empty()) {
        std::fprintf(stderr, "missing --script\n");
        return 2;
    }
    FILE* sf = fopen(script_path.c_str(), "r");
    if (!sf) {
        std::fprintf(stderr, "cannot open script %s\n", script_path.c_str());
        return 2;
    }

    // The lab cart is pure PRG: only the IRQ machinery is under test.
    nes23mmc3::Mmc3 mmc(Cart{{}, {}});
    std::string log;
    long total_edges = 0;
    bool prev_irq = false;

    auto emit_irq_rises = [&](void) {
        bool now = mmc.irq_line();
        if (now && !prev_irq) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "IRQ@%ld", total_edges);
            log += buf;
            log += '\n';
        }
        prev_irq = now;
    };

    char line[256];
    while (fgets(line, sizeof(line), sf)) {
        char op[16];
        unsigned addr = 0, val = 0;
        long n = 1;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%15s", op) != 1) continue;
        if (strcmp(op, "wr") == 0 &&
            sscanf(line, "%*s %x %x", &addr, &val) == 2) {
            mmc.cpu_write(uint16_t(addr), uint8_t(val));
            emit_irq_rises();
        } else if (strcmp(op, "edge") == 0) {
            sscanf(line, "%*s %ld", &n);  // default 1 when absent
            for (long k = 0; k < n; ++k) {
                ++total_edges;
                mmc.a12_edge();
                emit_irq_rises();
            }
        } else if (strcmp(op, "snap") == 0) {
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                          "edge=%ld cnt=%d latch=%d en=%d irq=%d",
                          total_edges, mmc.counter(), mmc.latch(),
                          int(mmc.enabled()), int(mmc.irq_line()));
            log += buf;
            log += '\n';
        }
    }
    fclose(sf);

    uint64_t h = kFnvOffset;
    for (char c : log) {
        h ^= uint8_t(c);
        h *= kFnvPrime;
    }
    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        fwrite(log.data(), 1, log.size(), tf);
        fclose(tf);
    }
    if (!hash_path.empty()) {
        FILE* hf = fopen(hash_path.c_str(), "wb");
        if (!hf) return 2;
        std::fprintf(hf, "%016llX", (unsigned long long)h);
        fclose(hf);
    }
    std::printf("fnv64=%016llX\nlines=%zu\nedges=%ld\n",
                (unsigned long long)h,
                size_t(std::count(log.begin(), log.end(), '\n')),
                total_edges);
    return 0;
}
