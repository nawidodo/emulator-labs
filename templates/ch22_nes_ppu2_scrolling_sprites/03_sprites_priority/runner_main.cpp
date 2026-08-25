// Headless runner for Chapter 22 — same mandatory CLI shape as ch21.
// `--rom` loads a NESF v1 snapshot; the renderer applies loopy scrolling,
// sprite evaluation, priority and sprite-0 hit detection.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "fixture.hpp"
#include "render22.hpp"

namespace {

constexpr uint64_t kDotsPerFullFrame = 341u * 262;

uint64_t cycles_for_frame(uint32_t index) {
    // Rendering enabled: odd frames skip one dot on the pre-render line.
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
                "usage: ch22_03_sprites_runner --rom FILE.nesf [flags]\n"
                "  --rom PATH        NESF v1 crafted PPU-state snapshot\n"
                "  --headless        always headless; accepted for parity\n"
                "  --cycles N        stop after N total PPU dots\n"
                "  --frames N        render N frames (default 1)\n"
                "  --trace FILE      write per-frame trace lines\n"
                "  --hash-frame PATH write final frame RGBA8 + print FNV64\n"
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

    nes22fix::Snapshot snap;
    std::string err;
    if (!nes22fix::read_nesf_file(rom.c_str(), snap, err)) {
        std::fprintf(stderr, "fixture error: %s\n", err.c_str());
        return 2;
    }

    nes22prio::Scene scene;
    scene.mirroring = static_cast<nes22prio::Mirroring>(snap.mirroring & 1);
    scene.chr = snap.chr.data();
    scene.nt = snap.nt.data();
    scene.pal = snap.pal.data();
    scene.oam = snap.oam.data();
    scene.l.v = snap.v;
    scene.l.t = snap.t;
    scene.l.x = snap.fine_x & 7;
    scene.l.w = false;
    scene.ctrl = snap.ctrl;
    scene.mask = snap.mask;

    std::array<uint8_t, 256 * 240 * 4> frame{};
    bool hit = nes22prio::render_frame(frame, scene);

    // Static scene: every frame is byte-identical, so the framebuffer comes
    // from one render; --frames/--cycles drive deterministic accounting.
    long rendered = frames;
    uint64_t total = 0;
    for (long f = 0; f < rendered; ++f) {
        uint64_t c = cycles_for_frame(uint32_t(f));
        if (cycle_cap >= 0 && total + c > uint64_t(cycle_cap)) {
            rendered = f;
            break;
        }
        total += c;
    }

    uint64_t fnv = nes22fix::fnv1a64(frame.data(), frame.size());
    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        for (long f = 0; f < rendered; ++f) {
            char line[96];
            int n = std::snprintf(line, sizeof(line),
                                  "frame=%ld hash=%016llX cyc=%llu\n", f,
                                  (unsigned long long)fnv,
                                  (unsigned long long)cycles_for_frame(
                                      uint32_t(f)));
            fwrite(line, 1, size_t(n), tf);
        }
        fclose(tf);
    }
    if (!hash_path.empty()) {
        FILE* hf = fopen(hash_path.c_str(), "wb");
        if (!hf) return 2;
        fwrite(frame.data(), 1, frame.size(), hf);
        fclose(hf);
    }
    std::printf("fnv64=%016llX\nsprite0_hit=%d\ntotal_dots=%llu\n",
                (unsigned long long)fnv, hit ? 1 : 0,
                (unsigned long long)total);
    return 0;
}
