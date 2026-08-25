// Exercise 04 runner — renders a built-in Mode 7 demo frame deterministically
// and writes the raw 256x224x4 RGBA image for golden hashing.
//
// Flags (chapter-standard set; unused ones are accepted no-ops):
//   --rom PATH        accepted as documented no-op: this runner's scene is
//                     built into the binary so the exercise needs no fixture
//   --headless        accepted, always headless
//   --cycles N        accepted no-op (no CPU is emulated)
//   --frames N        render N identical frames (default 1); the last is dumped
//   --trace FILE      accepted no-op (see chapter README: no CPU trace exists)
//   --hash-frame FILE write the raw RGBA frame here
//   --input-file FILE accepted no-op

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <span>
#include <string>

#include "mode7.hpp"

namespace {

struct Options {
    std::string rom;
    std::string trace;
    std::string hash;
    std::string input;
    long cycles = 0;
    long frames = 1;
};

int usage(const char* argv0) {
    std::printf(
        "usage: %s [--rom PATH] [--headless] [--cycles N] [--frames N]\n"
        "         [--trace FILE] [--hash-frame FILE] [--input-file FILE]\n",
        argv0);
    return 0;
}

bool parse_args(int argc, char** argv, Options* opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char** out) -> bool {
            if (i + 1 >= argc) {
                return false;
            }
            *out = argv[++i];
            return true;
        };
        if (a == "--rom") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->rom = v;
        } else if (a == "--trace") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->trace = v;
        } else if (a == "--hash-frame") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->hash = v;
        } else if (a == "--input-file") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->input = v;
        } else if (a == "--cycles") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->cycles = std::atol(v);
        } else if (a == "--frames") {
            const char* v = nullptr;
            if (!next(&v)) return false;
            opt->frames = std::atol(v);
            if (opt->frames < 1) opt->frames = 1;
        } else if (a == "--headless") {
            // always headless
        } else if (a == "--help" || a == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

void vbyte(snesbus::Vram& v, size_t byte_addr, uint8_t value) {
    uint16_t& w = v.w[(byte_addr >> 1) & 0x7FFFu];
    w = (byte_addr & 1u)
            ? static_cast<uint16_t>((w & 0x00FFu) | (value << 8))
            : static_cast<uint16_t>((w & 0xFF00u) | value);
}

// Built-in Mode 7 scene: a checkerboard map with a radial gradient per tile,
// rendered with a slight rotation and zoom, unwrapped.
void build_scene(snesbus::Vram& vram, snesbus::Cgram& cgram,
                 snesbus::Mode7Params& params, uint16_t* map_base) {
    *map_base = 0x1800;  // word address 0x1800 -> byte 0x3000
    const size_t base = static_cast<size_t>(*map_base) * 2u;

    // Tiles 1..200: each is a solid color derived from its number.
    for (unsigned t = 1; t <= 200; ++t) {
        const size_t tile_base = t * 64u;
        const uint8_t color =
            static_cast<uint8_t>(t % 256u == 0 ? 255 : t % 256u);
        for (unsigned i = 0; i < 64; ++i) {
            vbyte(vram, tile_base + i, color);
        }
    }
    // Checkerboard map of those tiles; tile 0 (transparent/backdrop) never
    // appears because the checker uses only odd sums.
    for (unsigned ty = 0; ty < 128; ++ty) {
        for (unsigned tx = 0; tx < 128; ++tx) {
            const unsigned n =
                ((tx / 4u + ty / 4u) & 1u) != 0 ? (tx + ty) % 200u + 1u
                                                : (ty + tx * 3u) % 200u + 1u;
            vbyte(vram, base + ty * 128u + tx,
                  static_cast<uint8_t>(n));
        }
    }

    // CGRAM: entry 0 backdrop (visible at extreme corners), identity-ish ramp.
    cgram.e[0] = 0x3800u;
    for (unsigned i = 1; i < 256; ++i) {
        const unsigned r = (i * 5u) & 31u;
        const unsigned g = (i * 3u) & 31u;
        const unsigned b = (i * 2u) & 31u;
        cgram.e[i] = static_cast<uint16_t>(r | (g << 5) | (b << 10));
    }

    // Slight rotation (~11 degrees) plus 1.25x zoom around screen center.
    params.a = 310;
    params.b = 60;
    params.c = -60;
    params.d = 310;
    params.x0 = 128;
    params.y0 = 112;
    params.hofs = 160;
    params.vofs = 140;
    params.wrap = false;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, &opt)) {
        usage(argv[0]);
        return 2;
    }

    snesbus::Vram vram;
    snesbus::Cgram cgram;
    snesbus::Mode7Params params;
    uint16_t map_base = 0;
    build_scene(vram, cgram, params, &map_base);

    std::array<uint8_t, snesbus::kScreenWidth * snesbus::kScreenHeight * 4> fb{};
    for (long f = 0; f < opt.frames; ++f) {
        snesbus::render_mode7_frame(params, vram, cgram, map_base,
                                    std::span<uint8_t>(fb));
    }

    if (!opt.hash.empty()) {
        std::ofstream out(opt.hash, std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", opt.hash.c_str());
            return 1;
        }
        out.write(reinterpret_cast<const char*>(fb.data()),
                  static_cast<std::streamsize>(fb.size()));
        std::printf("wrote %s (%zu bytes)\n", opt.hash.c_str(), fb.size());
    }
    return 0;
}
