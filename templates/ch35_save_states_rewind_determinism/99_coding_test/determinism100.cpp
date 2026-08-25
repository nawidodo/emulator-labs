// ch35 coding test: load-state-100-times determinism.
//   ch35_99_determinism100 --rom PATH --out FILE [--help]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "chip8.hpp"
#include "serialize.hpp"

namespace {

uint64_t fnv64(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* out_path = nullptr;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) {
            std::printf(
                "usage: ch35_99_determinism100 --rom PATH --out FILE\n"
                "\nLoads a state 100 times; all 100 later framebuffer\n"
                "hashes must be identical.\n");
            return 0;
        }
        if (!std::strcmp(argv[i], "--rom") && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) {
            out_path = argv[++i];
        }
    }
//@LABS-STUB
    // TODO(1): parse --rom PATH and --out FILE the same way (--help
    // prints usage, returns 0).
    (void)rom_path;
    (void)out_path;
//@LABS-END

    if (!rom_path || !out_path) {
        std::fprintf(stderr, "error: --rom and --out required (--help)\n");
        return 2;
    }

    // Phase A: boot from ROM, run 30 frames, snapshot state.
    chip8::Machine boot;
    boot.reset();
    {
        std::ifstream in(rom_path, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "error: cannot open rom '%s'\n", rom_path);
            return 2;
        }
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        boot.load(rom);
    }
    for (int f = 0; f < 30; ++f) boot.frame();
    std::vector<uint8_t> blob(chip8::kStateSize);
    if (chip8::write_state(boot, blob) == 0) {
        std::fprintf(stderr, "error: serialization not implemented\n");
        return 3;
    }

    uint64_t want = 0;
    bool ok = true;
//@LABS-BEGIN 2
//@LABS-SOLUTION
    for (int trial = 0; trial < 100; ++trial) {
        chip8::Machine m;  // fresh machine EVERY trial
        m.reset();
        if (!chip8::read_state(blob, m)) return 4;
        for (int f = 0; f < 60; ++f) m.frame();
        const uint64_t h =
            fnv64(m.fb.data(), chip8::kW * chip8::kH);
        if (trial == 0) {
            want = h;
        } else if (h != want) {
            ok = false;
        }
    }
//@LABS-STUB
    // TODO(2): repeat 100 times — fresh machine, read_state(blob), run 60
    // frames, hash the raw framebuffer with fnv64. Trial 0 sets `want`;
    // any later mismatch sets ok=false.
    (void)want;
    (void)ok;
    return 5;  // wrong on purpose: test never runs
//@LABS-END

    std::ofstream out(out_path, std::ios::binary);
    if (ok) {
        char line[64];
        std::snprintf(line, sizeof(line),
                      "determinism=ok hash=%016llX\n",
                      static_cast<unsigned long long>(want));
        out << line;
        std::printf("%s", line);
        return 0;
    }
    out << "determinism=mismatch\n";
    std::printf("determinism=mismatch\n");
    return 1;
}
