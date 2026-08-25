#pragma once
// Exercise 01 — the VRAM model and the three block-transfer commands.
//
// The PlayStation GPU owns 1 MiByte of dual-ported VRAM organized as 512
// scanlines of 1024 16-bit halfwords (1024x512 pixels in 15-bit BGR555).
// The CPU cannot address VRAM directly; it reaches it exclusively through
// GP0 block-transfer commands:
//
//   GP0(A0h)  CPU -> VRAM upload
//   GP0(C0h)  VRAM -> CPU download
//   GP0(80h)  VRAM -> VRAM copy
//
// All three share one addressing rule: coordinates wrap modulo the VRAM
// dimensions with NO carry from X into Y (PSX-SPX "Wrapping" note), and all
// three share the size normalisation ((size-1) AND mask)+1, which turns a
// size field of 0 into the maximum 1024x512.
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace psx::gpu {

inline constexpr int kVramWidth = 1024;  // halfwords per scanline
inline constexpr int kVramHeight = 512;  // scanlines

struct Vram {
    std::array<uint16_t, kVramWidth * kVramHeight> pix{};

    uint16_t& at(int x, int y);
    uint16_t at(int x, int y) const;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Linear index of pixel (x,y). Coordinates wrap: X is masked to 10 bits and
// Y to 9 bits, exactly like the hardware transfer address counters. A write
// past column 1023 therefore lands on column 0 of the same row — there is
// never a carry-out from X to Y nor from Y to X.
inline constexpr int vram_index(int x, int y) {
    return ((y & (kVramHeight - 1)) * kVramWidth) + (x & (kVramWidth - 1));
}

inline uint16_t& Vram::at(int x, int y) { return pix[vram_index(x, y)]; }
inline uint16_t Vram::at(int x, int y) const { return pix[vram_index(x, y)]; }
//@LABS-STUB
// TODO(1): mask X to 10 bits and Y to 9 bits, then compute
// row-major offset (y * kVramWidth + x) and use it for both at() overloads.
inline constexpr int vram_index(int x, int y) {
    (void)x;
    (void)y;
    return -1;  // wrong on purpose
}
inline uint16_t& Vram::at(int, int) {
    static uint16_t sink = 0;
    return sink;  // wrong on purpose
}
inline uint16_t Vram::at(int, int) const { return 0; }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// COPY size normalisation (PSX-SPX "Masking for COPY Commands parameters"):
//   Xsiz=((Xsiz-1) AND 3FFh)+1   Ysiz=((Ysiz-1) AND 1FFh)+1
// A parameter of 0 wraps around its whole field and degenerates to the
// maximum transfer (1024 wide / 512 high).
inline constexpr uint32_t copy_width(uint32_t w) {
    return ((w - 1u) & 0x3FFu) + 1u;
}
inline constexpr uint32_t copy_height(uint32_t h) {
    return ((h - 1u) & 0x1FFu) + 1u;
}
//@LABS-STUB
// TODO(2): implement the two PSX-SPX formulas above so that size 0 becomes
// the maximum (1024 / 512) and any other value is clipped to range.
inline constexpr uint32_t copy_width(uint32_t w) {
    (void)w;
    return 0;  // wrong on purpose
}
inline constexpr uint32_t copy_height(uint32_t h) {
    (void)h;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// GP0(A0h): stream w*h source halfwords row-major into VRAM at (x,y).
// The SOURCE stream stays linear (width x height); only DESTINATION
// addressing wraps at the VRAM edges. A row wider than the remaining
// columns continues at column 0 of the same destination row.
inline void cpu_to_vram(Vram& v, int x, int y, uint32_t w, uint32_t h,
                        std::span<const uint16_t> src) {
    w = copy_width(w);
    h = copy_height(h);
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c)
            v.at(x + static_cast<int>(c), y + static_cast<int>(r)) =
                src[r * w + c];
}
//@LABS-STUB
// TODO(3): normalise w/h, then copy each source pixel to destination
// (x+c, y+r), letting vram_index() provide the wrap.
inline void cpu_to_vram(Vram&, int, int, uint32_t, uint32_t,
                        std::span<const uint16_t>) {
    // wrong on purpose: leaves VRAM untouched
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// GP0(C0h): gather w*h halfwords row-major starting at (x,y). The GPU
// latches these internally; GPUREAD then pops them as little-endian words
// (see exercise 02).
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
//@LABS-STUB
// TODO(4): normalise w/h and gather pixels row-major into the output vector.
inline std::vector<uint16_t> vram_to_cpu(const Vram&, int, int, uint32_t,
                                         uint32_t) {
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// GP0(80h): copy within VRAM. The real GPU reads source data into internal
// latches (128 halfwords per chunk) before writing, so overlapping copies
// never smear source data across rows — snapshotting first models that.
inline void vram_to_vram(Vram& v, int sx, int sy, int dx, int dy, uint32_t w,
                         uint32_t h) {
    const std::vector<uint16_t> tmp =
        vram_to_cpu(v, sx, sy, copy_width(w), copy_height(h));
    cpu_to_vram(v, dx, dy, copy_width(w), copy_height(h), tmp);
}
//@LABS-STUB
// TODO(5): snapshot the source block via vram_to_cpu, then write it at the
// destination via cpu_to_vram (both sizes normalised).
inline void vram_to_vram(Vram&, int, int, int, int, uint32_t, uint32_t) {
    // wrong on purpose
}
//@LABS-END

}  // namespace psx::gpu
