// ch05 coding-test harness — DXY0 "scroll display" probe.
//
//   ch05_99_dxy0_tool --pattern NAME --scroll N [--quirk wrap|clip]
//       [--hash-frame FILE]
//
// Fills the display with a fixed pattern, executes ONE DXY0 instruction
// (scroll amount taken from register V6), prints the resulting VF as
// "vf=<0|1>", and optionally writes the frame as RGBA8888.
// Exit code is 0 whenever arguments parse; behaviour is judged via stdout
// and frame hashes.
#include "scroll_machine.hpp"
#include "frame_io.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int usage() {
    std::printf(
        "ch05 DXY0 coding-test probe\n"
        "usage: ch05_99_dxy0_tool --pattern NAME --scroll N [flags]\n"
        "\n"
        "  --pattern NAME     row_top | checker | bottom_row\n"
        "  --scroll N         value placed in register V6 (the scroll amount)\n"
        "  --quirk MODE       clip (default) or wrap; scroll must ignore it,\n"
        "                     this flag exists to prove that\n"
        "  --hash-frame FILE  write final display as RGBA8888\n"
        "\n"
        "Prints: vf=<0|1>\n");
    return 0;
}

void fill_pattern(chip8::Display& d, const std::string& name) {
    if (name == "row_top") {
        for (int x = 0; x < chip8::kWidth; ++x) d.set(x, 0, true);
    } else if (name == "bottom_row") {
        for (int x = 0; x < chip8::kWidth; ++x)
            d.set(x, chip8::kHeight - 1, true);
    } else if (name == "checker") {
        for (int y = 0; y < chip8::kHeight; ++y)
            for (int x = 0; x < chip8::kWidth; ++x)
                if ((x + y) % 2 == 0) d.set(x, y, true);
    } else {
        std::fprintf(stderr, "error: unknown pattern '%s'\n", name.c_str());
        std::exit(2);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string pattern, hash_path, quirk = "clip";
    long long scroll = -1;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        const auto next = [&]() -> const char* {
            return (k + 1 < argc) ? argv[++k] : nullptr;
        };
        const auto take = [&](std::string* dest) -> bool {
            const char* v = next();
            if (!v) return false;
            *dest = v;
            return true;
        };
        if (a == "--help" || a == "-h") return usage();
        else if (a == "--pattern") { if (!take(&pattern)) return usage(); }
        else if (a == "--scroll") { if (const char* v = next()) scroll = std::strtoll(v, nullptr, 0); }
        else if (a == "--quirk") { if (!take(&quirk)) return usage(); }
        else if (a == "--hash-frame") { if (!take(&hash_path)) return usage(); }
        else { std::fprintf(stderr, "unknown flag: %s\n", a.c_str()); return usage(); }
    }
    if (pattern.empty() || scroll < 0) return usage();

    // One-instruction program: DXY0 with y = 6 (amount in V6).
    const std::vector<uint8_t> prog = {0xD0, 0x60};
    chip8::Machine m;
    m.load(prog);
    fill_pattern(m.display, pattern);
    if (quirk == "wrap") m.quirks.wrapping = true;
    m.v[6] = static_cast<uint8_t>(scroll);

    m.run(1);  // exactly one instruction: the DXY0

    std::printf("vf=%d\n", m.v[0xF]);
    if (!hash_path.empty()) {
        const std::vector<uint8_t> buf = chip8::frame_bytes(m.display);
        if (!chip8::write_byte_file(hash_path, buf.data(), buf.size())) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         hash_path.c_str());
            return 2;
        }
    }
    return 0;
}
