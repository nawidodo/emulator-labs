// Headless runner for exercise 04: renders a full frame from a PPU
// snapshot image. Mandatory flag shape (docs/AUTHORING.md):
//   --rom PATH --headless --cycles N --frames N --trace FILE --hash-frame FILE
// For this display-less PPU rig `--rom` loads the PPU state image; every
// frame rendered is identical, so --frames only repeats work.
#include <cstdio>
#include <cstring>
#include <string>

#include "ppu.hpp"

namespace {

void usage() {
    std::printf(
        "ch14_04_frame_runner — headless GB PPU snapshot renderer\n"
        "usage: ch14_04_frame_runner --rom SNAPSHOT.ppu [--frames N]\n"
        "       [--hash-frame OUT.rgba] [--trace FILE] [--cycles N]\n"
        "       [--input-file FILE] [--headless]\n"
        "\n"
        "extensions over the common CLI:\n"
        "  none (BG-only frame renderer)\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string romPath, hashPath;
    int frames = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--rom") romPath = next();
        else if (a == "--hash-frame") hashPath = next();
        else if (a == "--frames") frames = std::atoi(next());
        else if (a == "--headless" || a == "--cycles" || a == "--input-file") {
            if (a != "--headless") ++i;  // accepted for CLI-shape parity
        } else if (a == "--trace") {
            // Trace of mode/scanline state arrives with chapter 15's PPU II.
            ++i;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    if (romPath.empty()) { usage(); return 2; }

    gbppu::PpuState st{};
    if (!gbppu::loadState(romPath, st)) {
        std::fprintf(stderr, "bad or missing snapshot: %s\n", romPath.c_str());
        return 1;
    }
    static gbppu::Frame frame;
    for (int f = 0; f < frames; ++f) gbppu::renderFrame(st, frame);
    if (!hashPath.empty()) {
        FILE* out = std::fopen(hashPath.c_str(), "wb");
        if (!out) { std::perror("hash-frame"); return 1; }
        std::fwrite(frame, 1, sizeof(frame), out);
        std::fclose(out);
    }
    std::printf("rendered %d frame(s) from %s\n", frames, romPath.c_str());
    return 0;
}
