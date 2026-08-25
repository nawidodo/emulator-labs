#pragma once
// NESF v1 snapshot format — identical layout to ch21's fixture (see
// templates/ch21_nes_ppu1/03_render_frame/fixture.hpp); duplicated here so
// chapters build independently. This copy exposes loopy v/t/x/w which the
// ch22 renderer consumes.
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace nes22fix {

constexpr size_t kNesfSize = 10542;

struct Snapshot {
    uint8_t mirroring = 0;   // 0=horizontal, 1=vertical
    uint8_t ctrl = 0;
    uint8_t mask = 0;
    uint8_t oam_addr = 0;
    uint8_t fine_x = 0;
    uint16_t v = 0;
    uint16_t t = 0;
    std::array<uint8_t, 0x2000> chr{};
    std::array<uint8_t, 0x0800> nt{};
    std::array<uint8_t, 0x0020> pal{};
    std::array<uint8_t, 0x0100> oam{};
};

inline bool parse_nesf(const std::vector<uint8_t>& blob, Snapshot& out,
                       std::string& err) {
    if (blob.size() != kNesfSize) {
        err = "size mismatch (want 10542)";
        return false;
    }
    if (blob[0] != 'N' || blob[1] != 'E' || blob[2] != 'S' || blob[3] != 'F') {
        err = "bad magic";
        return false;
    }
    if (blob[4] != 1) {
        err = "unsupported version";
        return false;
    }
    out.mirroring = blob[5];
    out.ctrl = blob[6];
    out.mask = blob[7];
    out.oam_addr = blob[8];
    out.fine_x = blob[9];
    out.v = uint16_t(blob[10] | (blob[11] << 8));
    out.t = uint16_t(blob[12] | (blob[13] << 8));
    size_t o = 14;
    for (size_t i = 0; i < 0x2000; ++i) out.chr[i] = blob[o++];
    for (size_t i = 0; i < 0x0800; ++i) out.nt[i] = blob[o++];
    for (size_t i = 0; i < 0x0020; ++i) out.pal[i] = blob[o++];
    for (size_t i = 0; i < 0x0100; ++i) out.oam[i] = blob[o++];
    return true;
}

inline bool read_nesf_file(const char* path, Snapshot& out, std::string& err) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        err = "cannot open " + std::string(path);
        return false;
    }
    std::vector<uint8_t> blob;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        blob.insert(blob.end(), buf, buf + n);
    fclose(f);
    return parse_nesf(blob, out, err);
}

inline uint64_t fnv1a64(const uint8_t* data, size_t n) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

}  // namespace nes22fix
