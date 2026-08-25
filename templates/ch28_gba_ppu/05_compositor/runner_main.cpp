// Headless GBA PPU runner (curriculum §52 CLI shape).
//
//   gba_ppu_runner --rom scene.pps [--frames N] [--headless]
//                  [--trace FILE] [--hash-frame FILE] [--input-file FILE]
//
// `--rom` loads a .pps script: little-endian records of {u32 addr, u16 value}
// applied as MMIO/VRAM writes before rendering, terminated by addr
// 0xFFFFFFFF. Rendering is deterministic; --hash-frame writes the FNV-1a 64
// digest of the final RGBA8888 frame.
#include <cstdio>
#include <cstring>
#include <string>
#include "ppu.hpp"

using namespace gba;

namespace {

void usage() {
    std::printf(
        "gba_ppu_runner — headless GBA PPU compositor\n"
        "usage: gba_ppu_runner --rom FILE [options]\n"
        "  --rom PATH          .pps MMIO-write script (required)\n"
        "  --headless          accepted for CLI compatibility (no-op)\n"
        "  --frames N          render N identical frames (default 1)\n"
        "  --trace FILE        write one trace line per scanline\n"
        "  --hash-frame FILE   write FNV-64 of the last frame to FILE\n"
        "  --input-file FILE   accepted for CLI compatibility (no input model)\n");
}

bool load_script(const char* path, gba::PpuMemory& m) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    while (true) {
        u8 rec[6];
        if (std::fread(rec, 1, 6, f) != 6) {
            std::fclose(f);
            return false;  // missing terminator
        }
        u32 addr = u32(rec[0]) | u32(rec[1]) << 8 | u32(rec[2]) << 16 |
                   u32(rec[3]) << 24;
        if (addr == 0xFFFFFFFFu) break;
        u16 value = u16(rec[4]) | u16(rec[5]) << 8;
        m.wr16(addr, value);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom = nullptr;
    const char* trace = nullptr;
    const char* hash = nullptr;
    int frames = 1;
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
            if (a == "--input-file" && !next(&rom)) {}
            // Accepted for cross-chapter CLI parity; no input model here.
        } else if (a == "--rom") {
            if (!next(&rom)) return 2;
        } else if (a == "--trace") {
            if (!next(&trace)) return 2;
        } else if (a == "--hash-frame") {
            if (!next(&hash)) return 2;
        } else if (a == "--frames") {
            if (i + 1 >= argc) return 2;
            frames = std::atoi(argv[++i]);
        } else {
            usage();
            return 2;
        }
    }
    if (!rom) {
        usage();
        return 2;
    }

    gba::PpuMemory m;
    if (!load_script(rom, m)) {
        std::fprintf(stderr, "error: cannot load script %s\n", rom);
        return 1;
    }

    std::FILE* tf = trace ? std::fopen(trace, "w") : nullptr;
    std::vector<u32> frame(kScreenW * kScreenH);
    for (int fnum = 0; fnum < frames; ++fnum) {
        compose_frame(m, frame.data());
        if (tf) {
            for (int y = 0; y < kScreenH; ++y)
                std::fprintf(tf, "op=scan vc=%02X cyc=%u\n", y,
                             unsigned(fnum) * 228u * 1232u +
                                 unsigned(y) * 1232u);
        }
    }
    if (tf) std::fclose(tf);

    if (hash) {
        u64 h = fnv64(frame.data(), frame.size() * sizeof(u32));
        FILE* hf = std::fopen(hash, "w");
        if (!hf) return 1;
        std::fprintf(hf, "%016llX\n", (unsigned long long)h);
        std::fclose(hf);
    }
    std::printf("rendered %d frame(s)\n", frames);
    return 0;
}
