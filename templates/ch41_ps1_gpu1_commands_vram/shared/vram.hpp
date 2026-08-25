#pragma once
// psx::gpu VRAM model — 1 MiByte, 1024x512 halfwords, 15-bit BGR pixels.
//
// Primary reference: PSX-SPX "GPU Memory Transfer Commands" and
// "Masking and Rounding for FILL/COPY Command parameters".
// https://problemkaputt.de/psx-spx.htm
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace psx::gpu {

inline constexpr int kVramWidth = 1024;   // halfwords per scanline
inline constexpr int kVramHeight = 512;   // scanlines (16bpp -> 1 MiB)

// Hardware VRAM address counters mask X to 10 bits and Y to 9 bits. A pixel
// past column 1023 reappears at column 0 of the SAME row, and a row past 511
// reappears at the top — there is never a carry-out from X into Y (PSX-SPX
// "Wrapping" note under the COPY commands).
inline constexpr int vram_index(int x, int y) {
    return ((y & (kVramHeight - 1)) * kVramWidth) + (x & (kVramWidth - 1));
}

struct Vram {
    std::array<uint16_t, kVramWidth * kVramHeight> pix{};

    uint16_t& at(int x, int y) { return pix[vram_index(x, y)]; }
    uint16_t at(int x, int y) const { return pix[vram_index(x, y)]; }
};

// PSX-SPX "Masking for COPY Commands parameters":
//   Xsiz=((Xsiz-1) AND 3FFh)+1   Ysiz=((Ysiz-1) AND 1FFh)+1
// i.e. sizes are clipped to their field width and Size=0 degenerates to
// Size=max (1024x512). Used by GP0(80h), GP0(A0h) and GP0(C0h).
inline constexpr uint32_t copy_width(uint32_t w) { return ((w - 1u) & 0x3FFu) + 1u; }
inline constexpr uint32_t copy_height(uint32_t h) { return ((h - 1u) & 0x1FFu) + 1u; }

// GP0(A0h) CPU->VRAM: stream w*h halfwords row-major starting at (x,y).
// The source stream is linear; only destination addressing wraps.
inline void cpu_to_vram(Vram& v, int x, int y, uint32_t w, uint32_t h,
                        std::span<const uint16_t> src) {
    w = copy_width(w);
    h = copy_height(h);
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c)
            v.at(x + static_cast<int>(c), y + static_cast<int>(r)) =
                src[r * w + c];
}

// GP0(C0h) VRAM->CPU: gather w*h halfwords row-major from (x,y).
inline std::vector<uint16_t> vram_to_cpu(const Vram& v, int x, int y,
                                         uint32_t w, uint32_t h) {
    w = copy_width(w);
    h = copy_height(h);
    std::vector<uint16_t> out;
    out.reserve(static_cast<size_t>(w) * h);
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c)
            out.push_back(v.at(x + static_cast<int>(c),
                               y + static_cast<int>(r)));
    return out;
}

// GP0(80h) VRAM->VRAM: copy w*h halfwords from (sx,sy) to (dx,dy).
// The real GPU latches source chunks (128 halfwords at a time) before
// writing, so an overlapping copy never smears: we snapshot first.
inline void vram_to_vram(Vram& v, int sx, int sy, int dx, int dy,
                         uint32_t w, uint32_t h) {
    const std::vector<uint16_t> tmp =
        vram_to_cpu(v, sx, sy, copy_width(w), copy_height(h));
    cpu_to_vram(v, dx, dy, copy_width(w), copy_height(h), tmp);
}

// PSX-SPX "Masking and Rounding for FILL Command parameters":
//   Xpos=(Xpos AND 3F0h)                      ;steps of 10h
//   Ypos=(Ypos AND 1FFh)
//   Xsiz=((Xsiz AND 3FFh)+0Fh) AND (NOT 0Fh)  ;rounded up to steps of 10h
//   Ysiz=(Ysiz AND 1FFh)
// Quirks preserved exactly:
//   - literal Xsiz=400h collapses to 400h AND 3FFh=0  => NO fill,
//   - but Xsiz=3F1h..3FFh rounds up to a genuine full-width 400h fill,
//   - fill does not occur when Xsiz=0 or Ysiz=0.
struct FillParams {
    int x;
    int y;
    uint32_t w;
    uint32_t h;
};

inline constexpr FillParams fill_params(uint32_t xpos, uint32_t ypos,
                                        uint32_t xsiz, uint32_t ysiz) {
    return {static_cast<int>(xpos & 0x3F0u),
            static_cast<int>(ypos & 0x1FFu),
            ((xsiz & 0x3FFu) + 0xFu) & ~0xFu,
            ysiz & 0x1FFu};
}

// GP0(02h): fill a rectangle with a constant color. Fill ignores the drawing
// area, the drawing offset and both mask bits (PSX-SPX); region coordinates
// wrap around the VRAM edges like every other block operation.
inline void fill_vram(Vram& v, uint16_t color16, uint32_t xpos, uint32_t ypos,
                      uint32_t xsiz, uint32_t ysiz) {
    const FillParams p = fill_params(xpos, ypos, xsiz, ysiz);
    if (p.w == 0 || p.h == 0) return;  // documented hardware no-op
    for (uint32_t r = 0; r < p.h; ++r)
        for (uint32_t c = 0; c < p.w; ++c)
            v.at(p.x + static_cast<int>(c), p.y + static_cast<int>(r)) =
                color16;
}

// 24-bit RGB (bits 0-7 R, 8-15 G, 16-23 B) -> 15-bit BGR555 with bit15=0.
// The GPU truncates each channel's low three bits when writing 16bpp data.
inline constexpr uint16_t rgb888_to_bgr555(uint32_t rgb) {
    const uint32_t r = (rgb >> 0) & 0xF8u;
    const uint32_t g = (rgb >> 8) & 0xF8u;
    const uint32_t b = (rgb >> 16) & 0xF8u;
    return static_cast<uint16_t>((r >> 3) | ((g >> 3) << 5) |
                                 ((b >> 3) << 10));
}

// Deterministic little-endian serialization of the full VRAM image; this is
// the byte stream that --hash-frame digests.
inline void serialize_vram(const Vram& v, std::vector<uint8_t>& out) {
    out.reserve(out.size() + v.pix.size() * 2);
    for (uint16_t px : v.pix) {
        out.push_back(static_cast<uint8_t>(px & 0xFFu));
        out.push_back(static_cast<uint8_t>(px >> 8));
    }
}

}  // namespace psx::gpu
