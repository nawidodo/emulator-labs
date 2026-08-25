// Challenge runner — loads a .sns scene bundle (--rom), renders it with the
// chapter renderer and writes the raw 256x224x4 RGBA image for hashing.
//
// Flags (chapter-standard set; unused ones are accepted no-ops):
//   --rom PATH        REQUIRED here: the .sns scene container (see scene.hpp)
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
#include <vector>

#include "scene.hpp"
#include "../03_bg_render/render.hpp"
#include "../04_mode7/mode7.hpp"

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
        "usage: %s --rom SCENE.sns [--headless] [--cycles N] [--frames N]\n"
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

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, &opt)) {
        usage(argv[0]);
        return 2;
    }
    if (opt.rom.empty()) {
        std::fprintf(stderr, "error: --rom PATH (.sns scene) is required\n");
        usage(argv[0]);
        return 2;
    }

    std::ifstream in(opt.rom, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open scene %s\n", opt.rom.c_str());
        return 1;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());

    snesbus::Scene scene;
    const std::string err =
        snesbus::load_scene(bytes.data(), bytes.size(), &scene);
    if (!err.empty()) {
        std::fprintf(stderr, "bad scene: %s\n", err.c_str());
        return 1;
    }

    std::array<uint8_t,
               snesbus::kScreenWidth * snesbus::kScreenHeight * 4>
        fb{};
    const auto span_fb = std::span<uint8_t>(fb);

    if (scene.regs.mode == 7) {
        snesbus::Mode7Params p;
        p.a = scene.regs.m7_a;
        p.b = scene.regs.m7_b;
        p.c = scene.regs.m7_c;
        p.d = scene.regs.m7_d;
        p.x0 = scene.regs.m7_x0;
        p.y0 = scene.regs.m7_y0;
        p.hofs = scene.regs.m7_hofs;
        p.vofs = scene.regs.m7_vofs;
        p.wrap = scene.regs.m7_wrap;
        for (long f = 0; f < opt.frames; ++f) {
            snesbus::render_mode7_frame(p, {scene.vram}, {scene.cgram},
                                        scene.regs.map_base[0], span_fb);
        }
    } else {
        snesbus::FrameCfg cfg;
        cfg.mode = scene.regs.mode == 0 ? snesbus::Mode::Mode0
                                        : snesbus::Mode::Mode1;
        for (int i = 0; i < 4; ++i) {
            const int src = i < 3 ? i : 2;  // Mode 0 BG4 reuses BG3 config slot
            cfg.bg[i] = snesbus::LayerCfg{
                scene.regs.mode == 0 ? uint8_t(2) : uint8_t(i < 2 ? 4 : 2),
                scene.regs.tile_base[src], scene.regs.map_base[src],
                scene.regs.hofs[src], scene.regs.vofs[src]};
        }
        cfg.window.enable = scene.regs.win_enable;
        cfg.window.invert = scene.regs.win_invert;
        cfg.window.left = scene.regs.win_left;
        cfg.window.right = scene.regs.win_right;
        cfg.window.layer_mask = scene.regs.win_layer_mask;
        cfg.window.color_math_enable = scene.regs.win_cmath_enable;
        cfg.color_math = scene.regs.cmath_enable;
        cfg.math_op = scene.regs.cmath_sub ? snesbus::MathOp::Sub
                                           : snesbus::MathOp::Add;
        cfg.math_half = scene.regs.cmath_half;

        const snesbus::Vram vram{scene.vram};
        const snesbus::Cgram cgram{scene.cgram};
        for (long f = 0; f < opt.frames; ++f) {
            snesbus::render_frame(cfg, vram, cgram, span_fb);
        }
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
