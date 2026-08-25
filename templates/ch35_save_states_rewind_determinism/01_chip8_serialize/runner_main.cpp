// Headless CHIP-8 runner (curriculum §52 shape) with save-state support:
//   ch35_01_chip8_runner --rom PATH --frames N
//                        [--load-state FILE] [--save-state FILE]
//                        [--frame-out FILE] [--trace FILE] [--help]
//
//
// One frame = 10 instructions + one 60 Hz DT/ST tick.
#include <cstdint>
#include <cstdio>
#include <cstring>
// One frame = 10 instructions + one 60 Hz DT/ST tick.
#include <fstream>
#include <iterator>
#include <vector>

#include "chip8.hpp"
#include "serialize.hpp"

namespace {

int print_help() {
    std::printf(
        "usage: ch35_01_chip8_runner --rom PATH --frames N\n"
        "       [--load-state F] [--save-state F] [--frame-out F]\n"
        "\n"
        "Runs the synthetic CHIP-8 core headlessly. --load-state replaces\n"
        "the ROM boot; --save-state writes after the run; --frame-out\n"
        "dumps the raw 64x32 framebuffer.\n");
    return 0;
}

bool read_file(const char* path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* load_path = nullptr;
    const char* save_path = nullptr;
    const char* frame_path = nullptr;
    int frames = 60;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) return print_help();
        if (!std::strcmp(argv[i], "--rom") && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--load-state") && i + 1 < argc) {
            load_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--save-state") && i + 1 < argc) {
            save_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--frame-out") && i + 1 < argc) {
            frame_path = argv[++i];
        }
    }

    chip8::Machine m;
    m.reset();

    if (load_path) {
        std::vector<uint8_t> blob;
        if (!read_file(load_path, blob) ||
            !chip8::read_state(blob, m)) {
            std::fprintf(stderr, "error: bad or missing state '%s'\n",
                         load_path);
            return 2;
        }
    } else if (rom_path) {
        std::vector<uint8_t> rom;
        if (!read_file(rom_path, rom)) {
            std::fprintf(stderr, "error: cannot open rom '%s'\n", rom_path);
            return 2;
        }
        m.load(rom);
    } else {
        std::fprintf(stderr, "error: --rom or --load-state required (--help)\n");
        return 2;
    }

    for (int f = 0; f < frames; ++f) m.frame();

    if (save_path) {
        std::vector<uint8_t> blob(chip8::kStateSize);
        chip8::write_state(m, blob);
        std::ofstream out(save_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(blob.data()),
                  std::streamsize(blob.size()));
        if (!out) {
            std::fprintf(stderr, "error: cannot write state '%s'\n",
                         save_path);
            return 2;
        }
    }
    if (frame_path) {
        std::ofstream out(frame_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(m.fb.data()),
                  std::streamsize(m.fb.size()));
    }

    std::printf("frames=%d pc=%03X hash=%016llX\n", frames, m.pc,
                static_cast<unsigned long long>(chip8::state_hash(m)));
    return 0;
}
