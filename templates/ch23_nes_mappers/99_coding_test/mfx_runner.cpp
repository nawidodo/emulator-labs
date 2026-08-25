// MFX-1 op-script runner (course-original fixture grammar):
//
//   ch23_99_mfx_runner --rom CART.nes --script FILE [--trace OUT]
//
// Script grammar (one op per line, '#' comments):
//   wr <hexaddr> <hexval>   CPU write through the mapper
//   rd <hexaddr>            CPU read;  logs "rd <addr>=<hh>"
//   prd <hexaddr>           PPU read;  logs "prd <addr>=<hh>"
//   snap                    logs "mfx r0=<hh> r1=<hh> r2=<hh> r3=<hh>
//                                wc=<d> irq=<d>" (one line, lowercase)
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ines.hpp"
#include "mfx.hpp"

namespace {
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;

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
}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, script_path, trace_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf(
                "usage: ch23_99_mfx_runner --rom CART.nes --script FILE\n"
                "       [--trace OUT]\n");
            return 0;
        } else if (a == "--rom") rom_path = next();
        else if (a == "--script") script_path = next();
        else if (a == "--trace") trace_path = next();
        else if (a == "--headless") {}
        else { std::fprintf(stderr, "unknown flag %s\n", a.c_str()); return 2; }
    }
    std::vector<uint8_t> blob;
    nes23cart::Cart cart;
    std::string err;
    if (!read_file(rom_path, blob) ||
        !nes23cart::load_cart(blob, cart, err)) {
        std::fprintf(stderr, "bad cart: %s\n",
                     err.empty() ? "unreadable" : err.c_str());
        return 2;
    }
    FILE* sf = fopen(script_path.c_str(), "r");
    if (!sf) {
        std::fprintf(stderr, "cannot open script %s\n", script_path.c_str());
        return 2;
    }

    nes23mfx::Mfx1 mfx(cart);
    std::string log;
    char buf[96];
    char line[256];
    while (fgets(line, sizeof(line), sf)) {
        char op[16];
        unsigned addr = 0, val = 0;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%15s", op) != 1) continue;
        if (strcmp(op, "wr") == 0 &&
            sscanf(line, "%*s %x %x", &addr, &val) == 2) {
            mfx.cpu_write(uint16_t(addr), uint8_t(val));
        } else if (strcmp(op, "rd") == 0 &&
                   sscanf(line, "%*s %x", &addr) == 1) {
            std::snprintf(buf, sizeof(buf), "rd %04x=%02x", addr,
                          mfx.cpu_read(uint16_t(addr)));
            log += buf; log += '\n';
        } else if (strcmp(op, "prd") == 0 &&
                   sscanf(line, "%*s %x", &addr) == 1) {
            std::snprintf(buf, sizeof(buf), "prd %04x=%02x", addr,
                          mfx.ppu_read(uint16_t(addr)));
            log += buf; log += '\n';
        } else if (strcmp(op, "snap") == 0) {
            log += mfx.debug_snapshot();
            log += '\n';
        }
    }
    fclose(sf);
    uint64_t h = kFnvOffset;
    for (char c : log) {
        h ^= uint8_t(c);
        h *= kFnvPrime;
    }
    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        fwrite(log.data(), 1, log.size(), tf);
        fclose(tf);
    }
    std::printf("fnv64=%016llX\nlines=%zu\n",
                (unsigned long long)h,
                size_t(std::count(log.begin(), log.end(), '\n')));
    return 0;
}
