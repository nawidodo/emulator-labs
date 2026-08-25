// Headless runner for Chapter 21 (mandatory CLI shape, curriculum §52):
//
//   ch21_03_render_runner --rom SCENE.nesf --frames N --headless
//                         [--cycles M] [--trace FILE] [--hash-frame FILE]
//                         [--input-file FILE]
//
// `--rom` loads a NESF v1 PPU-state snapshot (see fixture.hpp). The scene is
// static, so every rendered frame is identical; --frames exercises the frame
// loop and deterministic cycle accounting. --cycles caps the PPU-dot budget
// across the run. --input-file is accepted but has no effect: this chapter's
// scenes carry no scripted input.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "render_fixture.hpp"

using nes21fix::Snapshot;

namespace {

constexpr uint64_t kDotsPerFullFrame = 341u * 262;  // 89342
uint64_t cycles_for_frame(uint32_t index) {
    // With rendering enabled the pre-render scanline loses one dot on odd
    // frames — reproduce that so cycle accounting matches real hardware.
    return (index % 2 == 1) ? kDotsPerFullFrame - 1 : kDotsPerFullFrame;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom, trace_path, hash_path;
    long frames = 1;
    long cycle_cap = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf(
                "usage: ch21_03_render_runner --rom FILE.nesf [flags]\n"
                "  --rom PATH        NESF v1 crafted PPU-state snapshot\n"
                "  --headless        always headless; accepted for parity\n"
                "  --cycles N        stop after N total PPU dots\n"
                "  --frames N        render N frames (default 1)\n"
                "  --trace FILE      write per-frame trace lines\n"
                "  --hash-frame PATH write final frame as raw RGBA8 + print FNV64\n"
                "  --input-file PATH accepted; static scenes have no input\n");
            return 0;
        } else if (a == "--rom") {
            rom = next();
        } else if (a == "--headless") {
            // no-op
        } else if (a == "--cycles") {
            cycle_cap = std::strtol(next(), nullptr, 0);
        } else if (a == "--frames") {
            frames = std::strtol(next(), nullptr, 0);
        } else if (a == "--trace") {
            trace_path = next();
        } else if (a == "--hash-frame") {
            hash_path = next();
        } else if (a == "--input-file") {
            next();  // accepted, unused (documented)
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return 2;
        }
    }

    Snapshot snap;
    std::string err;
    if (!nes21fix::read_nesf_file(rom.c_str(), snap, err)) {
        std::fprintf(stderr, "fixture error: %s\n", err.c_str());
        return 2;
    }

    std::vector<uint8_t> trace;
    std::array<uint8_t, 256 * 240 * 4> frame{};
    uint64_t total_cycles = 0;
    uint64_t frame_fnv = 0;
    for (long f = 0; f < frames && (cycle_cap < 0 ||
                                    total_cycles < uint64_t(cycle_cap));
         ++f) {
        nes21render::render_snapshot_frame(frame, snap);
        frame_fnv = nes21fix::fnv1a64(frame.data(), frame.size());
        uint64_t cyc = cycles_for_frame(uint32_t(f));
        total_cycles += cyc;
        char line[96];
        int n = std::snprintf(line, sizeof(line),
                              "frame=%ld dot=0 hash=%016llX cyc=%llu\n", f,
                              (unsigned long long)frame_fnv,
                              (unsigned long long)cyc);
        trace.insert(trace.end(), line, line + n);
    }

    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) {
            std::fprintf(stderr, "cannot write trace %s\n", trace_path.c_str());
            return 2;
        }
        fwrite(trace.data(), 1, trace.size(), tf);
        fclose(tf);
    }

    if (!hash_path.empty()) {
        FILE* hf = fopen(hash_path.c_str(), "wb");
        if (!hf) {
            std::fprintf(stderr, "cannot write frame %s\n", hash_path.c_str());
            return 2;
        }
        fwrite(frame.data(), 1, frame.size(), hf);
        fclose(hf);
    }

    std::printf("fnv64=%016llX\nsha_frames=%ld\ntotal_dots=%llu\n",
                (unsigned long long)frame_fnv, frames,
                (unsigned long long)total_cycles);
    return 0;
}
