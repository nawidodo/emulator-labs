// Headless runner for the ch33 challenge. Implements the mandatory CLI
// shape for the chapter:
//
//   ch33_91_challenge_runner --rom PATH --headless --cycles N --frames N
//                            --trace FILE --hash-frame FILE
//                            --input-file FILE
//
// --rom         S33N bundle (see bundle.hpp; NOT a commercial ROM)
// --headless    accepted, no-op (this runner is always headless)
// --cycles N    accepted, no-op: the challenge is scanline-driven, the
//               CPU/APU domains of exercise 03 are not simulated here
// --frames N    number of frames to run (default 1); artifacts reflect
//               the FINAL frame
// --trace FILE  per-line effect log of the final frame, one event per
//               line, lowercase key=value: "line=<n> chan=<c> reg=<hex>
//               val=<hex>"
// --hash-frame FILE  raw 224-byte per-line effect buffer of the final
//                    frame (graders hash these bytes with FNV-1a-64)
// --input-file FILE  accepted, no-op: no interactive inputs in this chapter
#include "bundle.hpp"
#include "challenge.hpp"
#include "hdma_core.hpp"
#include <array>
#include <cstdlib>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int print_help() {
    std::fputs(
        "ch33_91_challenge_runner -- SNES HDMA scanline-effect challenge\n"
        "\n"
        "Usage:\n"
        "  --rom PATH          input bundle (S33N container, see README.md)\n"
        "  --headless          accepted, no-op (always headless)\n"
        "  --cycles N          accepted, no-op (scanline-driven model)\n"
        "  --frames N          run N frames (default 1), artifacts = final frame\n"
        "  --trace FILE        write per-line effect log (key=value lines)\n"
        "  --hash-frame FILE   write raw 224-byte per-line effect buffer\n"
        "  --input-file FILE   accepted, no-op (no interactive input)\n",
        stdout);
    return 0;
}

bool write_file(const std::string& path,
                std::span<const uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()),
              long(bytes.size()));
    return bool(out);
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path, hash_path;
    int frames = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&]() -> const char* {
            return i + 1 < argc ? argv[++i] : nullptr;
        };
        if (arg == "--help") return print_help();
        if (arg == "--rom") {
            const char* v = next();
            if (!v) return 2;
            rom_path = v;
        } else if (arg == "--frames") {
            const char* v = next();
            if (!v) return 2;
            frames = std::atoi(v);
        } else if (arg == "--trace") {
            const char* v = next();
            if (!v) return 2;
            trace_path = v;
        } else if (arg == "--hash-frame") {
            const char* v = next();
            if (!v) return 2;
            hash_path = v;
        } else if (arg == "--headless" || arg == "--cycles" ||
                   arg == "--input-file") {
            // Documented no-op flags; --cycles/--input-file consume a value.
            if (arg != "--headless" && next() == nullptr) return 2;
        } else {
            std::fprintf(stderr, "unknown argument: %s (--help lists all)\n",
                         argv[i]);
            return 2;
        }
    }
    if (rom_path.empty()) {
        std::fprintf(stderr, "error: --rom is required (--help)\n");
        return 2;
    }
    if (frames < 1) frames = 1;

    std::ifstream in(rom_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot open bundle %s\n",
                     rom_path.c_str());
        return 1;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    snesdma::challenge::Bundle bundle;
    if (!snesdma::challenge::load_bundle(bytes, bundle)) {
        std::fprintf(stderr, "error: %s is not a valid S33N bundle\n",
                     rom_path.c_str());
        return 1;
    }

    uint16_t watch = 0x2100;
    auto channels =
        snesdma::challenge::parse_channels(bundle.config, watch);
    if (channels.size() != size_t(snesdma::challenge::kChannels)) {
        channels.resize(size_t(snesdma::challenge::kChannels));
    }

    snesdma::challenge::HdmaCore hdma;
    hdma.set_blob(&bundle.blob);
    for (int c = 0; c < snesdma::challenge::kChannels; ++c) {
        hdma.configure(c, channels[size_t(c)]);
    }

    // Final frame's artifacts.
    std::vector<snesdma::challenge::LineEffect> log;
    std::array<uint8_t, snesdma::challenge::kVisibleLines> buffer{};
    for (int f = 0; f < frames; ++f) {
        log.clear();
        buffer.fill(0);
        hdma.init();  // frame start: hardware rewinds every channel table
        for (int n = 0; n < snesdma::challenge::kVisibleLines; ++n) {
            // Ordering contract: HDMA line effects BEFORE the line draws.
            hdma.run_line(n, log);
        }
    }
    snesdma::challenge::build_effect_buffer(log, watch, buffer);

    if (!hash_path.empty() &&
        !write_file(hash_path, buffer)) {
        std::fprintf(stderr, "error: cannot write %s\n", hash_path.c_str());
        return 1;
    }
    if (!trace_path.empty()) {
        std::string text;
        char row[64];
        for (const auto& e : log) {
            std::snprintf(row, sizeof(row), "line=%d chan=%d reg=%04x val=%02x\n",
                          e.line, e.channel, unsigned(e.reg),
                          unsigned(e.value));
            text += row;
        }
        std::ofstream out(trace_path, std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "error: cannot write %s\n",
                         trace_path.c_str());
            return 1;
        }
        out.write(text.data(), long(text.size()));
    }
    return 0;
}
