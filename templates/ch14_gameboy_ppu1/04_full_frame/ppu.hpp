// ppu.hpp — full-frame background renderer over a PPU snapshot.
//
// Snapshot image format ("PPU state file", version 1, little-endian):
//
//   offset      size   content
//   0x0000      0x2000 VRAM ($8000-$9FFF)
//   0x2000      1      LCDC ($FF40)
//   0x2001      1      BGP  ($FF47)
//   0x2002      1      SCY  ($FF42)
//   0x2003      1      SCX  ($FF43)
//   0x2004      1      WY   ($FF4A)
//   0x2005      1      WX   ($FF4B)
//
// Total 8198 bytes. This exercise renders BG only; the window bits are
// introduced in exercise 05 and ignored here.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace gbppu {

constexpr int kScreenWidth = 160;
constexpr int kScreenHeight = 144;
constexpr size_t kSnapshotSize = 0x2006;

constexpr uint8_t kLcdcLcdOn = 0x80;
constexpr uint8_t kLcdcBgEnable = 0x01;
constexpr uint8_t kLcdcBgMapHi = 0x08;
constexpr uint8_t kLcdcTileUnsigned = 0x10;

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t lcdc, bgp, scy, scx, wy, wx;
};

// RGBA8 framebuffer, row-major, exactly 160*144*4 bytes when dumped.
using Frame = uint8_t[kScreenHeight][kScreenWidth][4];

// Load a snapshot image; returns false if unreadable or wrong size.
inline bool loadState(const std::string& path, PpuState& out) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[kSnapshotSize];
    const size_t got = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    if (got != kSnapshotSize) return false;
    for (size_t i = 0; i < 0x2000; ++i) out.vram[i] = buf[i];
    out.lcdc = buf[0x2000];
    out.bgp = buf[0x2001];
    out.scy = buf[0x2002];
    out.scx = buf[0x2003];
    out.wy = buf[0x2004];
    out.wx = buf[0x2005];
    return true;
//@LABS-STUB
    // TODO(1): open the file binary, require exactly kSnapshotSize bytes,
    // then copy VRAM + the six registers into `out`.
    (void)path;
    (void)out;
    return false;
//@LABS-END
}

// Render one scanline of the BG layer into rgba[0..159] (RGBA8).
// Combines exercises 01-03: tilemap fetch with scroll wrap, BGP mapping,
// grayscale ramp. LCD off or BG disabled => shade 0 for the whole line.
void renderScanline(const PpuState& s, int ly, uint8_t rgba[160][4]) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;
    const bool bgOn = (s.lcdc & kLcdcBgEnable) != 0;
    static const uint8_t kGray[4] = {255, 192, 96, 0};
    for (int x = 0; x < kScreenWidth; ++x) {
        uint8_t shade = 0;
        if (lcdOn && bgOn) {
            const int sx = (s.scx + x) & 0xFF;
            const int sy = (s.scy + ly) & 0xFF;
            const uint16_t mapBase =
                (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
            const uint8_t indexByte =
                s.vram[mapBase + (sy / 8) * 32 + (sx / 8)];
            uint16_t tileOff;
            if (s.lcdc & kLcdcTileUnsigned)
                tileOff = static_cast<uint16_t>(indexByte * 16);
            else
                tileOff = static_cast<uint16_t>(
                    0x1000 +
                    static_cast<int>(static_cast<int8_t>(indexByte)) * 16);
            const int px = sx & 7;
            const int py = sy & 7;
            const uint8_t idx =
                static_cast<uint8_t>(((s.vram[tileOff + 2 * py] >>
                                       (7 - px)) & 1) |
                                     (((s.vram[tileOff + 2 * py + 1] >>
                                        (7 - px)) & 1) << 1));
            shade = static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
        }
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade];
        rgba[x][3] = 255;
    }
//@LABS-STUB
    // TODO(2): per-pixel surface fetch -> BGP -> grayscale ramp.
    // LCD off or BG disabled => shade 0 for the whole line.
    (void)s;
    (void)ly;
    (void)rgba;
//@LABS-END
}

// Render all 144 visible scanlines. LCDC bit7 clear => whole screen white.
void renderFrame(const PpuState& s, Frame frame) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    for (int ly = 0; ly < kScreenHeight; ++ly)
        renderScanline(s, ly, frame[ly]);
//@LABS-STUB
    // TODO(3): loop ly = 0..143 calling renderScanline.
    (void)s;
    (void)frame;
//@LABS-END
}

}  // namespace gbppu
