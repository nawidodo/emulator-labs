#pragma once
// ch42 shared headless runner CLI. Every exercise runner is the same shape:
//
//   <runner> --rom PATH [--headless] [--cycles N] [--frames N]
//            [--trace FILE] [--hash-frame FILE]
//
// --rom is a raw stream of little-endian u32 GP0 words. --trace writes one
//   pc=<hex> op=<hex> cyc=<n>   (lowercase hex, whitespace-separated k=v)
// line per command, matching tools/labs/compare_trace.py. --hash-frame
// writes the deterministic "fnv64=<16HEX>" payload over the full VRAM dump.
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "gpu_state.hpp"

namespace psx::gpu {

inline void labs_runner_usage(std::FILE* out) {
    std::fprintf(out,
                 "usage: runner --rom PATH [--headless] [--cycles N] "
                 "[--frames N] [--trace FILE] [--hash-frame FILE]\n"
                 "  --rom PATH          raw little-endian u32 GP0 word stream\n"
                 "  --headless          accepted, no-op\n"
                 "  --cycles N          accepted, no-op (stream runs to EOF)\n"
                 "  --frames N          accepted, no-op (stream runs to EOF)\n"
                 "  --trace FILE        write per-command execution trace\n"
                 "  --hash-frame FILE   write fnv64 digest over the VRAM dump\n");
}

inline bool labs_read_word_stream(const std::string& path,
                                 std::vector<uint32_t>& words) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    uint8_t buf[4];
    size_t n;
    while ((n = std::fread(buf, 1, 4, f)) == 4) {
        words.push_back(static_cast<uint32_t>(buf[0]) |
                        (static_cast<uint32_t>(buf[1]) << 8) |
                        (static_cast<uint32_t>(buf[2]) << 16) |
                        (static_cast<uint32_t>(buf[3]) << 24));
    }
    const bool ok = n == 0;  // trailing partial word = corrupt fixture
    std::fclose(f);
    return ok;
}

template <class Dev>
int labs_runner_main(int argc, char** argv) {
    std::string rom_path, trace_path, hash_path;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") {
            labs_runner_usage(stdout);
            return 0;
        }
        if (a == "--headless") continue;
        auto value_of = [&](const char*& out) -> bool {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return true;
        };
        const char* val = nullptr;
        if (a == "--rom") {
            if (!value_of(val)) {
                labs_runner_usage(stderr);
                return 2;
            }
            rom_path = val;
        } else if (a == "--cycles" || a == "--frames") {
            if (!value_of(val)) {
                labs_runner_usage(stderr);
                return 2;
            }
            // Accepted for CLI compatibility; the stream always runs to EOF.
        } else if (a == "--trace") {
            if (!value_of(val)) {
                labs_runner_usage(stderr);
                return 2;
            }
            trace_path = val;
        } else if (a == "--hash-frame") {
            if (!value_of(val)) {
                labs_runner_usage(stderr);
                return 2;
            }
            hash_path = val;
        } else {
            std::fprintf(stderr, "error: unknown flag '%s'\n", a.c_str());
            labs_runner_usage(stderr);
            return 2;
        }
    }
    if (rom_path.empty()) {
        std::fprintf(stderr, "error: --rom is required\n");
        labs_runner_usage(stderr);
        return 2;
    }

    std::vector<uint32_t> words;
    if (!labs_read_word_stream(rom_path, words)) {
        std::fprintf(stderr, "error: cannot read GP0 stream '%s'\n",
                     rom_path.c_str());
        return 1;
    }

    auto dev_storage = std::make_unique<Dev>();
    Dev& dev = *dev_storage;
    for (size_t i = 0; i < words.size(); ++i)
        dev.feed(words[i], static_cast<uint32_t>(i * 4));

    if (!trace_path.empty()) {
        std::FILE* f = std::fopen(trace_path.c_str(), "w");
        if (f == nullptr) {
            std::fprintf(stderr, "error: cannot write trace '%s'\n",
                         trace_path.c_str());
            return 1;
        }
        for (size_t i = 0; i < dev.cmd_log.size(); ++i) {
            std::fprintf(f, "pc=%04x op=%08x cyc=%zu\n", dev.cmd_log[i].pc,
                         dev.cmd_log[i].word, i);
        }
        std::fclose(f);
    }
    if (!hash_path.empty()) {
        std::FILE* f = std::fopen(hash_path.c_str(), "w");
        if (f == nullptr) {
            std::fprintf(stderr, "error: cannot write hash-frame '%s'\n",
                         hash_path.c_str());
            return 1;
        }
        const std::string payload = hash_frame_payload(dev.vram);
        std::fwrite(payload.data(), 1, payload.size(), f);
        std::fclose(f);
    }
    return 0;
}

}  // namespace psx::gpu
