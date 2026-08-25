// Exercise 03 runner — renders a built-in Mode 1 demo frame deterministically
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
//   --input-file FILE accepted no-op (sprites are out of scope)

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <span>
#include <string>

#include "render.hpp"

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

// Build the deterministic demo scene: three Mode 1 layers of pattern tiles,
// one window rectangle with BG1 masked off, add-half color math.
void build_scene(snesbus::Vram& vram, snesbus::Cgram& cgram,
                 snesbus::FrameCfg& cfg) {
    using namespace snesbus;

    // --- tiles ---------------------------------------------------------
    // BG1 (4bpp @ word 0): four tiles, pixel color = ((r ^ c) + t) % 14 + 1.
    for (unsigned t = 0; t < 4; ++t) {
        const size_t base = t * 32u;
        for (int r = 0; r < 8; ++r) {
            uint8_t pl[4] = {0, 0, 0, 0};
            for (int c = 0; c < 8; ++c) {
                const unsigned px =
                    ((static_cast<unsigned>(r) ^ static_cast<unsigned>(c)) +
                     t) % 14u + 1u;
                for (unsigned p = 0; p < 4; ++p) {
                    pl[p] |= static_cast<uint8_t>(((px >> p) & 1u) << (7 - c));
                }
            }
            vbyte(vram, base + r * 2u, pl[0]);
            vbyte(vram, base + r * 2u + 1, pl[1]);
            vbyte(vram, base + 16u + r * 2u, pl[2]);
            vbyte(vram, base + 17u + r * 2u, pl[3]);
        }
    }
    // BG2 (4bpp @ word 512): two tiles forming vertical stripes.
    for (unsigned t = 0; t < 2; ++t) {
        const size_t base = 512u * 2u + t * 32u;
        for (int r = 0; r < 8; ++r) {
            uint8_t lo = 0;
            uint8_t hi = 0;
            for (int c = 0; c < 8; ++c) {
                const unsigned px =
                    (((c / 2u + t) % 2u) != 0 ? 9u : 10u);
                lo |= static_cast<uint8_t>((px & 1u) << (7 - c));
                hi |= static_cast<uint8_t>(((px >> 3) & 1u) << (7 - c));
            }
            vbyte(vram, base + r * 2u, lo);
            vbyte(vram, base + r * 2u + 1, 0);
            vbyte(vram, base + 16u + r * 2u, 0);
            vbyte(vram, base + 17u + r * 2u, hi);
        }
    }
    // BG3 (2bpp @ word 1024): single tile with a diagonal of color 3.
    for (int r = 0; r < 8; ++r) {
        uint8_t p0 = 0;
        uint8_t p1 = 0;
        for (int c = 0; c < 8; ++c) {
            const unsigned px = (r == c) ? 3u : 0u;
            p0 |= static_cast<uint8_t>((px & 1u) << (7 - c));
            p1 |= static_cast<uint8_t>(((px >> 1) & 1u) << (7 - c));
        }
        vbyte(vram, 1024u * 2u + r * 2u, p0);
        vbyte(vram, 1024u * 2u + r * 2u + 1, p1);
    }

    // --- tilemaps ------------------------------------------------------
    cfg.mode = Mode::Mode1;
    cfg.bg[0] = LayerCfg{4, 0, 2048, 13, 7};
    cfg.bg[1] = LayerCfg{4, 512, 3072, 250, 200};  // negative-looking scroll
    cfg.bg[2] = LayerCfg{2, 1024, 4096, 64, 32};
    for (unsigned i = 0; i < 1024; ++i) {
        const unsigned tx = i & 31u;
        const unsigned ty = i >> 5;
        vram.w[2048 + i] = static_cast<uint16_t>(
            0x2000u | (2u << 10) | (((tx * ty) & 3u)));  // prio, palette 2
        vram.w[3072 + i] = static_cast<uint16_t>(
            ((tx + ty) & 4u) << 10 | 512u + ((tx + ty) & 1u));
        vram.w[4096 + i] = static_cast<uint16_t>(
            (tx & 4u) && (ty & 4u) ? (8u << 10) : 0u);
    }

    // --- CGRAM ---------------------------------------------------------
    cgram.e[0] = 0x294Au;  // backdrop: muted blue-gray
    for (unsigned p = 0; p < 8; ++p) {
        for (unsigned c = 1; c < 16; ++c) {
            cgram.e[p * 16u + c] = static_cast<uint16_t>(
                ((c * 2u) & 31u) | (((p * 4u + 8u) & 31u) << 5) |
                (0x15u << 10));
        }
    }
    for (unsigned p = 0; p < 8; ++p) {  // mode-0 style band used by BG3? no:
        cgram.e[32u + p * 4u] = 0x0000u;
        cgram.e[32u + p * 4u + 1] = 0x0366u;
        cgram.e[32u + p * 4u + 2] = 0x06C9u;
        cgram.e[32u + p * 4u + 3] = 0x7BDEu;
    }

    // --- window + color math -------------------------------------------
    cfg.window.enable = true;
    cfg.window.invert = false;
    cfg.window.left = 48;
    cfg.window.right = 143;
    cfg.window.layer_mask = 0b0110;  // BG1 hidden inside the window
    cfg.window.color_math_enable = true;
    cfg.color_math = true;
    cfg.math_op = MathOp::Add;
    cfg.math_half = true;
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
    snesbus::FrameCfg cfg;
    build_scene(vram, cgram, cfg);

    std::array<uint8_t, snesbus::kScreenWidth * snesbus::kScreenHeight * 4> fb{};
    for (long f = 0; f < opt.frames; ++f) {
        snesbus::render_frame(cfg, vram, cgram, std::span<uint8_t>(fb));
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
