// Headless runner for exercise 04: renders full frames from a PPU state
// file v2 snapshot. Mandatory flag shape (docs/AUTHORING.md):
//   --rom PATH --headless --cycles N --frames N --trace FILE
//   --hash-frame FILE --input-file FILE --help
// Chapter extensions:
//   --window-off-lines A:B   disable the window for screen lines [A,B)
//                            while rendering (models a mid-frame toggle of
//                            LCDC bit 5; skipped lines do not advance the
//                            window's internal content counter)
// `--rom` loads the .ppu2 snapshot image; every rendered frame is
// identical, so --frames only repeats work. --trace writes the mode-
// transition log of the first frame.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstddef>

#include "ppu.hpp"

namespace {

void usage() {
    std::printf(
        "ch15_04_fullppu_runner — headless GB PPU v2 snapshot renderer\n"
        "usage: ch15_04_fullppu_runner --rom SNAPSHOT.ppu2 [--frames N]\n"
        "       [--hash-frame OUT.rgba] [--trace OUT.log]\n"
        "       [--window-off-lines A:B] [--cycles N] [--input-file FILE]\n"
        "       [--headless]\n"
        "\n"
        "extensions over the common CLI:\n"
        "  --window-off-lines A:B  window disabled for screen lines [A,B)\n"
        "  --trace                 mode-transition log of the first frame\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string romPath, hashPath, tracePath, offLines;
    int frames = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--rom") romPath = next();
        else if (a == "--hash-frame") hashPath = next();
        else if (a == "--trace") tracePath = next();
        else if (a == "--window-off-lines") offLines = next();
        else if (a == "--frames") frames = std::atoi(next());
        else if (a == "--headless" || a == "--cycles" ||
                 a == "--input-file") {
            if (a != "--headless") ++i;  // accepted for CLI-shape parity
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    if (romPath.empty()) { usage(); return 2; }

    gbppu2::PpuState st{};
    if (!gbppu2::loadState(romPath, st)) {
        std::fprintf(stderr, "bad or missing snapshot: %s\n",
                     romPath.c_str());
        return 1;
    }

    bool windowOn[gbppu2::kScreenHeight];
    for (bool& b : windowOn) b = true;
    if (!offLines.empty()) {
        const size_t colon = offLines.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "bad --window-off-lines: %s\n",
                         offLines.c_str());
            return 2;
        }
        const int a0 = std::atoi(offLines.substr(0, colon).c_str());
        const int b0 = std::atoi(offLines.substr(colon + 1).c_str());
        for (int ly = a0; ly < b0 && ly < gbppu2::kScreenHeight; ++ly)
            if (ly >= 0) windowOn[ly] = false;
    }

    static gbppu2::Frame frame;
    for (int f = 0; f < frames; ++f) gbppu2::renderFrame(st, frame, windowOn);

    if (!hashPath.empty()) {
        FILE* out = std::fopen(hashPath.c_str(), "wb");
        if (!out) { std::perror("hash-frame"); return 1; }
        std::fwrite(frame, 1, sizeof(frame), out);
        std::fclose(out);
    }
    if (!tracePath.empty()) {
        // Mode-transition log of the first rendered frame (deterministic).
        const std::string log = gbppu2::buildModeTrace();
        FILE* out = std::fopen(tracePath.c_str(), "wb");
        if (!out) { std::perror("trace"); return 1; }
        std::fwrite(log.data(), 1, log.size(), out);
        std::fclose(out);
    }
    std::printf("rendered %d frame(s) from %s\n", frames, romPath.c_str());
    return 0;
}
