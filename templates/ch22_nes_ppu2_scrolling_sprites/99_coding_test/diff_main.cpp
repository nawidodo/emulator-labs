// ch22_99_frame_diff — screenshot-diff diagnoser CLI.
//
//   ch22_99_frame_diff FRAME_A.rgba FRAME_B.rgba
//
// Prints the diagnostic report (grammar in frame_diff.hpp) to stdout.
#include <cstdio>
#include <string>
#include <cstddef>

#include "frame_diff.hpp"

namespace {

bool read_file(const char* path, std::string& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char buf[8192];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out.size() == size_t(nes22diff::kW) * nes22diff::kH * 4;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::printf(
            "usage: ch22_99_frame_diff FRAME_A.rgba FRAME_B.rgba\n"
            "Diffs two raw 256x240 RGBA8 frames and prints hash_a, hash_b,\n"
            "ndiff, first=x,y and shift=h1|v1|none|other.\n");
        return 0;
    }
    if (argc != 3) {
        std::fprintf(stderr, "expected exactly two frame files\n");
        return 2;
    }
    std::string a, b;
    if (!read_file(argv[1], a) || !read_file(argv[2], b)) {
        std::fprintf(stderr,
                     "frames must exist and be %d bytes (256x240 RGBA8)\n",
                     nes22diff::kW * nes22diff::kH * 4);
        return 2;
    }
    nes22diff::DiffReport r;
    nes22diff::count_diff(a, b, r);
    nes22diff::classify_shift(a, b, r);
    fputs(nes22diff::report_text(r).c_str(), stdout);
    return 0;
}
