// Headless cart-inspection runner (mandatory CLI shape, curriculum §52):
//
//   ch23_01_ines_runner --rom CART.nes [--headless] [--cycles N]
//                       [--frames N] [--trace FILE] [--hash-frame FILE]
//                       [--input-file FILE]
//
// Prints the parsed header summary and FNV-64 digests of the PRG/CHR
// slices, so hidden manifests can pin unseen synthetic carts. The static
// flags (--cycles/--frames/--input-file) are accepted for CLI uniformity
// and have no effect: a cartridge image is not a program.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ines.hpp"

using nes23cart::Cart;
using nes23cart::Mirroring;

namespace {
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;

uint64_t fnv1a64(const std::vector<uint8_t>& v) {
    uint64_t h = kFnvOffset;
    for (uint8_t b : v) {
        h ^= b;
        h *= kFnvPrime;
    }
    return h;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(size_t(n > 0 ? n : 0));
    bool ok = n == 0 || fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

const char* mirror_name(Mirroring m) {
    switch (m) {
        case Mirroring::Vertical: return "V";
        case Mirroring::FourScreen: return "4";
        default: return "H";
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::string rom, trace_path;
    // Accepted-for-shape flags; parsed and otherwise ignored.
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf("usage: ch23_01_ines_runner --rom CART.nes [flags]\n");
            return 0;
        } else if (a == "--rom") {
            rom = next();
        } else if (a == "--trace") {
            trace_path = next();
        } else if (a == "--headless" || a == "--cycles" || a == "--frames" ||
                   a == "--hash-frame" || a == "--input-file") {
            if (a != "--headless") next();
        } else {
            std::fprintf(stderr, "unknown flag %s\n", a.c_str());
            return 2;
        }
    }
    if (rom.empty()) {
        std::fprintf(stderr, "missing --rom\n");
        return 2;
    }

    std::vector<uint8_t> blob;
    if (!read_file(rom, blob)) {
        std::fprintf(stderr, "cannot read %s\n", rom.c_str());
        return 2;
    }
    Cart cart;
    std::string err;
    if (!nes23cart::load_cart(blob, cart, err)) {
        std::fprintf(stderr, "%s: %s\n", rom.c_str(), err.c_str());
        return 2;
    }

    char summary[160];
    std::snprintf(summary, sizeof(summary),
                  "mapper=%d prg_banks=%zu chr_banks=%zu mirroring=%s "
                  "prg_fnv64=%016llX chr_fnv64=%016llX\n",
                  cart.mapper, cart.prg.size() / 16384,
                  cart.chr.size() / 8192, mirror_name(cart.mirroring),
                  (unsigned long long)fnv1a64(cart.prg),
                  (unsigned long long)fnv1a64(cart.chr));
    std::fputs(summary, stdout);
    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        fwrite(summary, 1, strlen(summary), tf);
        fclose(tf);
    }
    return 0;
}
