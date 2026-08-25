#pragma once
// PPU-state snapshot loading (format spec in CODING_TEST.md).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../05_compositor/ppu.hpp"

namespace gba {

constexpr u32 kSnapIoLen = 0x100;
constexpr u32 kSnapPalLen = 0x400;
constexpr u32 kSnapVramLen = 0x18000;
constexpr u32 kSnapOamLen = 0x400;

// Serialize a full snapshot from memory state (used by fixture generators).
inline void save_snapshot(const PpuMemory& m, std::vector<u8>& out) {
    out.clear();
    auto push32 = [&](u32 v) {
        out.push_back(u8(v));
        out.push_back(u8(v >> 8));
        out.push_back(u8(v >> 16));
        out.push_back(u8(v >> 24));
    };
    const char magic[] = "GBASNP1";
    out.insert(out.end(), magic, magic + 8);
    push32(kSnapIoLen);
    push32(kSnapPalLen);
    push32(kSnapVramLen);
    push32(kSnapOamLen);
    out.insert(out.end(), m.io, m.io + kSnapIoLen);
    out.insert(out.end(), m.pal, m.pal + kSnapPalLen);
    out.insert(out.end(), m.vram, m.vram + kSnapVramLen);
    out.insert(out.end(), m.oam, m.oam + kSnapOamLen);
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Parse and validate a snapshot; returns false on any mismatch. Layout is
// documented in CODING_TEST.md: 8-byte magic, four little-endian u32 region
// lengths, then IO/PAL/VRAM/OAM payloads back to back.
inline bool load_snapshot(const u8* data, size_t len, PpuMemory& m) {
    const size_t header = 24;
    if (len < header || std::memcmp(data, "GBASNP1", 7) != 0) return false;
    auto rd32 = [&](size_t off) {
        return u32(data[off]) | u32(data[off + 1]) << 8 |
               u32(data[off + 2]) << 16 | u32(data[off + 3]) << 24;
    };
    if (rd32(8) != kSnapIoLen || rd32(12) != kSnapPalLen ||
        rd32(16) != kSnapVramLen || rd32(20) != kSnapOamLen)
        return false;
    size_t need =
        header + kSnapIoLen + kSnapPalLen + kSnapVramLen + kSnapOamLen;
    if (len != need && len < need) return false;
    size_t o = header;
    std::memcpy(m.io, data + o, kSnapIoLen);
    o += kSnapIoLen;
    std::memcpy(m.pal, data + o, kSnapPalLen);
    o += kSnapPalLen;
    std::memcpy(m.vram, data + o, kSnapVramLen);
    o += kSnapVramLen;
    std::memcpy(m.oam, data + o, kSnapOamLen);
    return true;
}
//@LABS-STUB
// TODO(1): parse the snapshot format (see CODING_TEST.md). Validate the
// "GBASNP1" magic, the four little-endian length fields, and that the
// payload is complete before copying each region into `mem`. Return false
// on ANY inconsistency.
inline bool load_snapshot(const u8* data, size_t len, PpuMemory& m) {
    (void)data;
    (void)len;
    (void)m;
    return false;  // wrong on purpose: rejects every snapshot
}
//@LABS-END

}  // namespace gba
