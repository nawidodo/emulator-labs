// Headless cartridge-bus runner for ch16. Mandatory flag shape
// (docs/AUTHORING.md) plus the chapter's documented op-script extension:
//
//   ch16_05_cart_runner --rom CART.gb [--headless] [--cycles N]
//       [--frames N] [--trace FILE] [--hash-frame FILE] [--input-file OPS]
//
// Op script (one op per line, '#' comments allowed):
//   W <hexaddr> <hexval>   bus write: $0000-$7FFF register window or
//                          $A000-$BFFF cart RAM / RTC
//   R <hexaddr>            bus read; echoes `R <hexaddr>=<hexval>` to
//                          stdout and to the --trace file
//   T <dec-cycles>         advance the injected-tick RTC (MBC3 only;
//                          other mappers ignore it)
// --hash-frame receives the same byte stream as --trace so golden FNV-64
// hashes can be computed with tools/labs/hash_frame.py.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstddef>

#include "cart.hpp"

namespace {

void usage() {
    std::printf(
        "ch16_05_cart_runner — headless GB cartridge/mapper runner\n"
        "usage: ch16_05_cart_runner --rom CART.gb [--headless]\n"
        "       [--cycles N] [--frames N] [--trace FILE]\n"
        "       [--hash-frame FILE] [--input-file OPS]\n"
        "\n"
        "op-script extension (see chapter README):\n"
        "  W <hexaddr> <hexval>   mapper/RAM write\n"
        "  R <hexaddr>            bus read, echoed `R <addr>=<val>`\n"
        "  T <dec-cycles>         inject RTC ticks (MBC3)\n");
}

std::vector<uint8_t> loadFile(const char* path) {
    std::vector<uint8_t> data;
    FILE* f = std::fopen(path, "rb");
    if (!f) return data;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        data.resize(static_cast<size_t>(n));
        if (std::fread(data.data(), 1, data.size(), f) != data.size())
            data.clear();
    }
    std::fclose(f);
    return data;
}

}  // namespace

int main(int argc, char** argv) {
    std::string romPath, tracePath, hashPath, opsPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--rom") romPath = next();
        else if (a == "--trace") tracePath = next();
        else if (a == "--hash-frame") hashPath = next();
        else if (a == "--input-file") opsPath = next();
        else if (a == "--cycles" || a == "--frames") ++i;   // accepted no-op
        else if (a == "--headless") {}                      // accepted
        else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    if (romPath.empty()) { usage(); return 2; }

    const std::vector<uint8_t> rom = loadFile(romPath.c_str());
    if (rom.size() < 0x150) {
        std::fprintf(stderr, "bad or missing rom: %s\n", romPath.c_str());
        return 1;
    }
    auto mapper = cart::CartridgeController::makeMapper(rom.data(),
                                                        rom.size());
    if (!mapper) { std::fprintf(stderr, "no mapper for cart\n"); return 1; }

    std::string out;                       // accumulated R-line stream
    if (!opsPath.empty()) {
        FILE* ops = std::fopen(opsPath.c_str(), "r");
        if (!ops) {
            std::fprintf(stderr, "cannot open op script: %s\n",
                         opsPath.c_str());
            return 1;
        }
        char line[256];
        while (std::fgets(line, sizeof(line), ops)) {
            char* p = line;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '#' || *p == '\n' || *p == '\0') continue;

            if ((p[0] == 'R' || p[0] == 'W' || p[0] == 'T') &&
                (p[1] == ' ' || p[1] == '\t')) {
                const uint32_t addr =
                    static_cast<uint32_t>(std::strtoul(p + 2, nullptr, 16));
                if (p[0] == 'T') {
                    // Decimal cycle injection into the RTC.
                    mapper->tickRtc(std::strtoull(p + 2, nullptr, 10));
                    continue;
                }
                if (p[0] == 'W') {
                    char* end = nullptr;
                    const uint32_t waddr =
                        static_cast<uint32_t>(std::strtoul(p + 2, &end, 16));
                    const uint8_t val = static_cast<uint8_t>(
                        std::strtoul(end, nullptr, 16));
                    if (waddr >= 0xA000)
                        mapper->writeRam(static_cast<uint16_t>(waddr), val);
                    else
                        mapper->writeReg(static_cast<uint16_t>(waddr),
                                         val);
                    continue;
                }
                const uint8_t val =
                    addr >= 0xA000
                        ? mapper->readRam(static_cast<uint16_t>(addr))
                        : mapper->readRom(static_cast<uint16_t>(addr));
                char buf[32];
                std::snprintf(buf, sizeof(buf), "R %04X=%02X\n", addr, val);
                out += buf;
                continue;
            }
            std::fprintf(stderr, "bad op line: %s", line);
            std::fclose(ops);
            return 2;
        }
        std::fclose(ops);
    }

    std::fputs(out.c_str(), stdout);   // each R op echoes to stdout too
    if (!tracePath.empty()) {
        FILE* t = std::fopen(tracePath.c_str(), "wb");
        if (!t) { std::perror("trace"); return 1; }
        std::fwrite(out.data(), 1, out.size(), t);
        std::fclose(t);
    }
    if (!hashPath.empty()) {
        FILE* h = std::fopen(hashPath.c_str(), "wb");
        if (!h) { std::perror("hash-frame"); return 1; }
        std::fwrite(out.data(), 1, out.size(), h);
        std::fclose(h);
    }
    return 0;
}
