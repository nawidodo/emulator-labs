#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Chapter 23 — cartridge container: iNES header parsing + PRG/CHR slicing.
//
// Every mapper in this chapter sits on top of the same cart image, so the
// parse has to be right before anything banks correctly. We support the
// classic iNES format only (16-byte header, optional 512-byte trainer,
// PRG ROM, CHR ROM). Mapper number combines BOTH header nibbles:
//
//   byte 6 high nibble | byte 7 low nibble   ->  mapper << 4 style merge
//
// i.e. mapper = (flags6 >> 4) | (flags7 & 0xF0).
namespace nes23cart {

enum class Mirroring : uint8_t { Horizontal = 0, Vertical = 1, FourScreen = 2 };

struct Cart {
    int mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    std::vector<uint8_t> prg;  // prg_banks * 16 KiB
    std::vector<uint8_t> chr;  // chr_banks * 8 KiB (may be empty: CHR RAM)
};

struct Header {
    uint8_t prg_banks = 0;  // 16 KiB units
    uint8_t chr_banks = 0;  // 8 KiB units
    int mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    bool has_trainer = false;

    size_t prg_bytes() const { return size_t(prg_banks) * 16384; }
    size_t chr_bytes() const { return size_t(chr_banks) * 8192; }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Parse the 16-byte iNES header. Returns false (and leaves `out` untouched
// fields defaulted) when the "NES\x1A" magic is missing.
inline bool parse_header(const uint8_t* raw, size_t len, Header& out) {
    if (len < 16 || raw[0] != 'N' || raw[1] != 'E' || raw[2] != 'S' ||
        raw[3] != 0x1A)
        return false;
    Header h;
    h.prg_banks = raw[4];
    h.chr_banks = raw[5];
    h.has_trainer = (raw[6] & 0x04) != 0;
    // Mirroring: bit 0 of flags6 names the arrangement, bit 3 (four-screen)
    // overrides it entirely when set.
    if (raw[6] & 0x08)
        h.mirroring = Mirroring::FourScreen;
    else
        h.mirroring =
            (raw[6] & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal;
    h.mapper = (raw[6] >> 4) | (raw[7] & 0xF0);
    out = h;
    return true;
}
//@LABS-STUB
// TODO(1): validate the magic, read prg/chr bank counts, derive the mapper
// number from BOTH nibble sources, resolve mirroring (four-screen wins).
inline bool parse_header(const uint8_t* raw, size_t len, Header& out) {
    (void)raw; (void)len; (void)out;
    return false;  // TODO(1)
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Split a full .nes blob into header + PRG + CHR, skipping the trainer when
// present. CHR may legitimately be absent (CHR-RAM boards such as UxROM).
inline bool load_cart(const std::vector<uint8_t>& blob, Cart& out,
                      std::string& err) {
    Header h;
    if (!parse_header(blob.data(), blob.size(), h)) {
        err = "bad iNES header";
        return false;
    }
    size_t off = 16;
    if (h.has_trainer) off += 512;
    Cart c;
    c.mapper = h.mapper;
    c.mirroring = h.mirroring;
    if (blob.size() < off + h.prg_bytes() + h.chr_bytes()) {
        err = "truncated cart image";
        return false;
    }
    c.prg.assign(blob.begin() + long(off),
                 blob.begin() + long(off + h.prg_bytes()));
    c.chr.assign(blob.begin() + long(off + h.prg_bytes()),
                 blob.begin() + long(off + h.prg_bytes() + h.chr_bytes()));
    out = std::move(c);
    return true;
}
//@LABS-STUB
// TODO(2): skip the trainer when flagged, slice PRG then CHR, reject
// truncated images.
inline bool load_cart(const std::vector<uint8_t>& blob, Cart& out,
                      std::string& err) {
    (void)blob; (void)out;
    err = "TODO(2)";
    return false;
}
//@LABS-END

}  // namespace nes23cart
