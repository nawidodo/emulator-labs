// Renders a PPU-state snapshot (CODING_TEST.md format) to a raw RGBA8888
// frame: ch28_snapshot_tool scene.snap out.rgba
#include <cstdio>
#include <string>
#include <vector>
#include "snapshot.hpp"
#include <cstddef>
using namespace gba;

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::printf(
            "usage: ch28_snapshot_tool SCENE.snap OUT.rgba\n"
            "  renders one 240x160 RGBA8888 frame from a PPU snapshot\n");
        return 0;
    }
    if (argc != 3) {
        std::printf(
            "usage: ch28_snapshot_tool SCENE.snap OUT.rgba\n"
            "  renders one 240x160 RGBA8888 frame from a PPU snapshot\n");
        return argc == 3 ? 0 : 2;
    }
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) {
        std::fprintf(stderr, "error: cannot open %s\n", argv[1]);
        return 1;
    }
    std::vector<u8> data;
    u8 buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        data.insert(data.end(), buf, buf + n);
    std::fclose(f);

    gba::PpuMemory m;
    if (!gba::load_snapshot(data.data(), data.size(), m)) {
        std::fprintf(stderr, "error: invalid snapshot\n");
        return 1;
    }
    std::vector<u32> frame(gba::kScreenW * gba::kScreenH);
    gba::compose_frame(m, frame.data());

    FILE* out = std::fopen(argv[2], "wb");
    if (!out) return 1;
    std::fwrite(frame.data(), 4, frame.size(), out);
    std::fclose(out);
    return 0;
}
