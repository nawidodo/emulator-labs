// runner_main.cpp — headless runner, curriculum §52 CLI shape.
//
//   ch03_99_coding_runner --rom FILE [--headless] [--cycles N] [--frames N]
//                       [--trace FILE] [--hash-frame FILE]
//
// --cycles/--frames are additive instruction counts this chapter (timer-paced
// frame scheduling arrives in Chapter 4). --hash-frame dumps the 64x32
// framebuffer as raw RGBA8 (lit pixel = FF FF FF FF, off = 00 00 00 00).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstddef>

#include "chip8.hpp"

namespace {

struct Options {
    const char* rom = nullptr;
    long cycles = -1;
    long frames = -1;
    const char* trace = nullptr;
    const char* hash_frame = nullptr;
};

void print_usage(const char* argv0) {
    std::printf(
        "usage: %s --rom FILE [--headless] [--cycles N] [--frames N]\n"
        "                      [--trace FILE] [--hash-frame FILE]\n"
        "\n"
        "  --rom FILE       CHIP-8 program to load at 0x200 (required)\n"
        "  --headless       accepted for CLI compatibility; always headless\n"
        "  --cycles N       execute N instructions\n"
        "  --frames N       execute N more instructions (see --cycles)\n"
        "  --trace FILE     write one trace line per step\n"
        "  --input-file F  accepted for CLI compat; keypad events land in ch05\n"
        "  --hash-frame FILE dump the 64x32 framebuffer as raw RGBA8\n",
        argv0);
}

void dump_frame_rgba(const chip8::Chip8& c, const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot write: %s\n", path);
        std::exit(1);
    }
    for (const auto& px : c.pixels()) {
        const unsigned char on = px ? 0xFF : 0x00;
        const unsigned char rgba[4] = {on, on, on, on};
        out.write(reinterpret_cast<const char*>(rgba), 4);
    }
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    bool want_help = false;
    for (int n = 1; n < argc; ++n) {
        const std::string a = argv[n];
        auto next = [&]() -> const char* {
            return (n + 1 < argc) ? argv[++n] : nullptr;
        };
        if (a == "--help" || a == "-h") want_help = true;
        else if (a == "--rom") opt.rom = next();
        else if (a == "--headless") {}
        else if (a == "--input-file") next();  // keypad scripting: chapter 5
        else if (a == "--cycles") opt.cycles = next() ? std::atol(argv[n]) : -2;
        else if (a == "--frames") opt.frames = next() ? std::atol(argv[n]) : -2;
        else if (a == "--trace") opt.trace = next();
        else if (a == "--hash-frame") opt.hash_frame = next();
        else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return 2;
        }
    }
    if (want_help || opt.rom == nullptr) {
        print_usage(argv[0]);
        return want_help ? 0 : 2;
    }

    std::ifstream rom_file(opt.rom, std::ios::binary);
    if (!rom_file) {
        std::fprintf(stderr, "cannot open rom: %s\n", opt.rom);
        return 1;
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(rom_file)),
                                     std::istreambuf_iterator<char>());

    chip8::Chip8 c;
    c.reset();
    c.load(bytes);

    long steps = 1000;  // deterministic default
    if (opt.cycles >= 0 || opt.frames >= 0) steps = 0;
    if (opt.cycles >= 0) steps += opt.cycles;
    if (opt.frames >= 0) steps += opt.frames;

    std::ofstream trace;
    if (opt.trace) trace.open(opt.trace);
    for (long n = 0; n < steps; ++n) {
        c.step();
        if (opt.trace) {
            char line[256];
            int off = std::snprintf(line, sizeof line, "pc=%04X op=%04X",
                                    c.pc(), c.last_op());
            for (int r = 0; r < 16; ++r)
                off += std::snprintf(line + off, sizeof(line) - off,
                                     " V%X=%02X", r, c.v(static_cast<size_t>(r)));
            std::snprintf(line + off, sizeof(line) - off,
                          " I=%03X SP=%02X DT=%02X ST=%02X cyc=%ld", c.i(),
                          c.sp(), c.delay(), c.sound(), n + 1);
            trace << line << "\n";
        }
    }

    if (opt.hash_frame) dump_frame_rgba(c, opt.hash_frame);

    std::printf("steps=%ld pc=%04X\n", steps, c.pc());
    return 0;
}
