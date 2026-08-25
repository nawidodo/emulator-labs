// Headless runner for exercise 05: full BG + window frame from a PPU
// snapshot image. Same mandatory CLI shape as exercise 04.
#include <cstdio>
#include <cstring>
#include <string>

#include "win.hpp"

namespace {

void usage() {
    std::printf(
        "ch14_05_window_runner — headless GB PPU snapshot renderer "
        "(BG+window)\n"
        "usage: ch14_05_window_runner --rom SNAPSHOT.ppu [--frames N]\n"
        "       [--hash-frame OUT.rgba] [--trace FILE] [--cycles N]\n"
        "       [--input-file FILE] [--headless]\n");
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
            if (a != "--headless") ++i;
        } else if (a == "--trace") {
            ++i;  // mode traces arrive in chapter 15
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    if (romPath.empty()) { usage(); return 2; }

    gbwin::PpuState st{};
    if (!gbwin::loadState(romPath, st)) {
        std::fprintf(stderr, "bad or missing snapshot: %s\n", romPath.c_str());
        return 1;
    }
    static gbwin::Frame frame;
    for (int f = 0; f < frames; ++f) {
        int wl = 0;
        for (int ly = 0; ly < gbwin::kScreenHeight; ++ly) {
            gbwin::renderScanline(st, ly, wl, frame[ly]);
            if (gbwin::windowActive(st, ly)) ++wl;
        }
    }
    if (!hashPath.empty()) {
        FILE* out = std::fopen(hashPath.c_str(), "wb");
        if (!out) { std::perror("hash-frame"); return 1; }
        std::fwrite(frame, 1, sizeof(frame), out);
        std::fclose(out);
    }
    std::printf("rendered %d frame(s) from %s\n", frames, romPath.c_str());
    return 0;
}
