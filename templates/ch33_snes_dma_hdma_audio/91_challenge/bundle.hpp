#pragma once
// S33N bundle loader for chapter 33 runners ("--rom" means input BUNDLE
// here; no commercial ROMs are ever used).
//
// Container layout, all integers little-endian:
//
//   offset  size  field
//   0       4     magic "S33N" (0x53 0x33 0x33 0x4E)
//   4       1     version, must be 1
//   5       3     reserved, must be 0
//   8       4     config_len (bytes)
//   12      N     config text (ASCII, see below)
//   12+N    4     blob_len (bytes)
//   16+N    M     blob: HDMA tables + indirect data, addressed by offset
//
// Config grammar: one key=value per line; blank lines and lines starting
// with '#' are ignored. Recognized keys:
//
//   watch=RRRR        hex $21xx register tracked into the effect buffer
//   chN.enable=0|1    channel present at all
//   chN.reg=RRRR      hex base $21xx register written each active line
//   chN.regs=K        consecutive registers per line, 1..4
//   chN.indirect=0|1  direct table vs indirect pointers
//   chN.bank=BB       hex bank byte used for indirect addressing
//   chN.table=OOOO:LL hex offset/length of this channel's table in blob
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace snesdma::challenge {

struct Bundle {
    std::string config;
    std::vector<uint8_t> blob;
};

inline bool load_bundle(std::span<const uint8_t> bytes, Bundle& out) {
    if (bytes.size() < 16) return false;
    const uint8_t magic[4] = {0x53, 0x33, 0x33, 0x4E};
    for (int i = 0; i < 4; ++i) {
        if (bytes[size_t(i)] != magic[i]) return false;
    }
    if (bytes[4] != 1) return false;  // version
    if (bytes[5] != 0 || bytes[6] != 0 || bytes[7] != 0) return false;
    const uint32_t config_len = uint32_t(bytes[8]) | uint32_t(bytes[9]) << 8 |
                                uint32_t(bytes[10]) << 16 |
                                uint32_t(bytes[11]) << 24;
    if (size_t(12) + config_len + 4 > bytes.size()) return false;
    out.config.assign(reinterpret_cast<const char*>(&bytes[12]), config_len);
    const size_t tail = 12 + size_t(config_len);
    const uint32_t blob_len = uint32_t(bytes[tail]) |
                              uint32_t(bytes[tail + 1]) << 8 |
                              uint32_t(bytes[tail + 2]) << 16 |
                              uint32_t(bytes[tail + 3]) << 24;
    if (tail + 4 + blob_len > bytes.size()) return false;
    out.blob.assign(bytes.begin() + long(tail + 4),
                    bytes.begin() + long(tail + 4 + blob_len));
    return true;
}

inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

inline int parse_hex(std::string_view s) {
    int v = 0;
    for (const char c : s) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = v * 16 + d;
    }
    return v;
}

}  // namespace snesdma::challenge
