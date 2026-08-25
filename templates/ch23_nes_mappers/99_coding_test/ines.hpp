#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Chapter 23 coding test — iNES loading for unseen MFX-1 carts.
// (Same parser contract as 01_ines_header; duplicated per exercise dir.)
namespace nes23cart {

enum class Mirroring : uint8_t { Horizontal = 0, Vertical = 1, FourScreen = 2 };

struct Header {
    uint8_t prg_banks = 0;
    uint8_t chr_banks = 0;
    int mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    bool has_trainer = false;

    size_t prg_bytes() const { return size_t(prg_banks) * 16384; }
    size_t chr_bytes() const { return size_t(chr_banks) * 8192; }
};

struct Cart {
    int mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr;
};

inline bool parse_header(const uint8_t* raw, size_t len, Header& out) {
    if (len < 16 || raw[0] != 'N' || raw[1] != 'E' || raw[2] != 'S' ||
        raw[3] != 0x1A)
        return false;
    Header h;
    h.prg_banks = raw[4];
    h.chr_banks = raw[5];
    h.has_trainer = (raw[6] & 0x04) != 0;
    if (raw[6] & 0x08)
        h.mirroring = Mirroring::FourScreen;
    else
        h.mirroring =
            (raw[6] & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal;
    h.mapper = (raw[6] >> 4) | (raw[7] & 0xF0);
    out = h;
    return true;
}

inline bool load_cart(const std::vector<uint8_t>& blob, Cart& out,
                      std::string& err) {
    Header h;
    if (!parse_header(blob.data(), blob.size(), h)) {
        err = "bad iNES header";
        return false;
    }
    size_t off = 16;
    if (h.has_trainer) off += 512;
    if (blob.size() < off + h.prg_bytes() + h.chr_bytes()) {
        err = "truncated cart image";
        return false;
    }
    Cart c;
    c.mapper = h.mapper;
    c.mirroring = h.mirroring;
    c.prg.assign(blob.begin() + long(off),
                 blob.begin() + long(off + h.prg_bytes()));
    c.chr.assign(blob.begin() + long(off + h.prg_bytes()),
                 blob.begin() + long(off + h.prg_bytes() + h.chr_bytes()));
    out = std::move(c);
    return true;
}

}  // namespace nes23cart
