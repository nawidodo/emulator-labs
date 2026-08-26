// Challenge runner — headless GPU command-stream executor.
//
// Stream format (--rom): raw little-endian u32 words, interpreted as a GP0
// word stream (port 1F801810h). GP1 display-control commands cannot be
// distinguished from GP0 packets on a raw wire (both live in the same
// opcode space), so the harness reserves the UNDOCUMENTED GP0 opcode 12h
// as an escape: the word 12000000h means "the NEXT word is a raw GP1
// command (opcode in bits 31-24)". Fixture data must simply avoid that one
// word value (our assembler guarantees it).
//
// The stream is processed --frames N times into a zero-initialised VRAM;
// afterwards --hash-frame writes "fnv64=<16 uppercase hex>\n" computed with
// FNV-1a-64 over the full little-endian 1024x512x2-byte VRAM image.
#include <cstdint>
#include <memory>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../shared/fnv.hpp"
#include "../shared/vram.hpp"
#include "gpu.hpp"

namespace {

int usage(std::FILE* out) {
    std::fprintf(out,
                 "usage: ch41_91_challenge_runner --rom FILE [--frames N] "
                 "[--cycles N] [--headless]\n"
                 "                               [--trace FILE] "
                 "[--hash-frame FILE]\n");
    return 2;
}

bool load_words(const char* path, std::vector<uint32_t>& words) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    uint8_t buf[4];
    while (std::fread(buf, 1, 4, f) == 4) {
        words.push_back(uint32_t(buf[0]) | uint32_t(buf[1]) << 8 |
                        uint32_t(buf[2]) << 16 | uint32_t(buf[3]) << 24);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom = nullptr;
    const char* trace_path = nullptr;
    const char* hash_path = nullptr;
    long frames = 1;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            usage(stdout);
            return 0;
        } else if (arg == "--rom" && i + 1 < argc) {
            rom = argv[++i];
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--hash-frame" && i + 1 < argc) {
            hash_path = argv[++i];
        } else if ((arg == "--frames" || arg == "--cycles") &&
                   i + 1 < argc) {
            const long v = std::strtol(argv[++i], nullptr, 10);
            if (v < 0) return usage(stderr);
            if (arg == "--frames") frames = v;
            // --cycles: accepted for CLI-shape compatibility; the GPU model
            // is cycle-accurate-free (commands execute synchronously).
        } else if (arg == "--headless") {
            // accepted, no-op
        } else {
            return usage(stderr);  // unknown flag -> nonzero exit
        }
    }
    if (rom == nullptr) return usage(stderr);

    std::vector<uint32_t> words;
    if (!load_words(rom, words)) {
        std::fprintf(stderr, "cannot read rom: %s\n", rom);
        return 2;
    }

    std::FILE* trace = nullptr;
    if (trace_path != nullptr) {
        trace = std::fopen(trace_path, "wb");
        if (trace == nullptr) {
            std::fprintf(stderr, "cannot write trace: %s\n", trace_path);
            return 2;
        }
    }

    auto gpu_storage = std::make_unique<psx::gpu::Gpu>();
    psx::gpu::Gpu& gpu = *gpu_storage;
    gpu.reset();
    bool gp1_escape = false;
    for (long frame = 0; frame < frames; ++frame) {
        for (size_t w = 0; w < words.size(); ++w) {
            const uint32_t word = words[w];
            if (gp1_escape) {
                gpu.write_gp1(word);
                gp1_escape = false;
            } else if (word == 0x12000000u) {
                gp1_escape = true;  // GP1 escape (reserved GP0 opcode 12h)
            } else {
                gpu.write_gp0(word);
            }
            if (trace != nullptr)
                std::fprintf(trace, "pc=%zx op=%08x cyc=%zu\n",
                             w * sizeof(uint32_t), word, w + 1);
        }
    }
    if (trace != nullptr) std::fclose(trace);

    int rc = 0;
    if (hash_path != nullptr) {
        std::vector<uint8_t> dump;
        psx::gpu::serialize_vram(gpu.vram, dump);
        std::FILE* h = std::fopen(hash_path, "wb");
        if (h == nullptr) {
            std::fprintf(stderr, "cannot write hash: %s\n", hash_path);
            return 2;
        }
        std::fprintf(h, "fnv64=%s\n",
                     psx::gpu::fnv64_hex(dump).c_str());
        std::fclose(h);
    }
    return rc;
}
