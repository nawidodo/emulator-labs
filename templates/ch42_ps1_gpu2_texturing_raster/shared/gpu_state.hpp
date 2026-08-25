#pragma once
// ch42 shared GPU state — infrastructure only (no @LABS blocks).
//
// Bit layouts and formulas follow PSX-SPX (problemkaputt.de/psx-spx.htm):
//   GP0(E1h) Draw Mode / Texpage, GP0(E2h) Texture Window,
//   GP0(E3h/E4h) Drawing Area, GP0(E5h) Drawing Offset, GP0(E6h) Mask Bits.
// Exercises supply the rendering stages (texture fetch, blending, clipping);
// everything here is plumbing the whole chapter shares.
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <string>

namespace psx::gpu {

constexpr int kVramWidth = 1024;   // halfwords per line
constexpr int kVramHeight = 512;   // lines
constexpr int kVramHalfwords = kVramWidth * kVramHeight;

// 1 MiB of VRAM as 16-bit pixels. Coordinates wrap modulo the VRAM size,
// which is what the real chip's address counters effectively do.
struct Vram {
    uint16_t px[kVramHalfwords] = {};

    uint16_t& at(int x, int y) {
        return px[(static_cast<unsigned>(y) & (kVramHeight - 1)) * kVramWidth +
                  (static_cast<unsigned>(x) & (kVramWidth - 1))];
    }
    const uint16_t& at(int x, int y) const {
        return px[(static_cast<unsigned>(y) & (kVramHeight - 1)) * kVramWidth +
                  (static_cast<unsigned>(x) & (kVramWidth - 1))];
    }
};

// FNV-1a 64 — identical to tools/labs/grade.py. Every golden hash in this
// course is this digest, so tests pin binary VRAM state without depending
// on the Python tooling at runtime.
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;

inline uint64_t fnv64_bytes(const uint8_t* data, size_t n) {
    uint64_t h = kFnvOffset;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= kFnvPrime;
    }
    return h;
}

// Serialize the dump explicitly little-endian so the digest does not depend
// on host byte order.
inline uint64_t fnv64_vram(const Vram& v) {
    uint64_t h = kFnvOffset;
    for (int i = 0; i < kVramHalfwords; ++i) {
        const uint16_t hw = v.px[i];
        h ^= static_cast<uint8_t>(hw & 0xFF);
        h *= kFnvPrime;
        h ^= static_cast<uint8_t>(hw >> 8);
        h *= kFnvPrime;
    }
    return h;
}

// Payload written by --hash-frame. Depends on ALL VRAM state and nothing else.
inline std::string hash_frame_payload(const Vram& v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "fnv64=%016llX\n",
                  static_cast<unsigned long long>(fnv64_vram(v)));
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Decoded rendering attributes.
// ---------------------------------------------------------------------------

// CLUT location in VRAM. x is already scaled to halfwords (the register
// field counts in 16-halfword steps).
struct Clut {
    int x = 0;
    int y = 0;
};

struct DrawMode {           // GP0(E1h) / Texpage attribute
    int page_x_field = 0;   // raw 4-bit field; page base X = field * 64 halfwords
    int page_y_base = 0;    // 0 or 256
    int semi = 0;           // 0=B/2+F/2  1=B+F  2=B-F  3=B+F/4
    int depth = 0;          // 0=4bpp CLUT  1=8bpp CLUT  2/3=15bpp direct
    bool dither = false;    // GPUSTAT.9
};

struct TexWindow {          // GP0(E2h), all fields in 8-texel steps
    int mask_x = 0;
    int mask_y = 0;
    int off_x = 0;
    int off_y = 0;
};

struct DrawArea {           // GP0(E3h) top-left, GP0(E4h) bottom-right
    int x1 = 0;
    int y1 = 0;
    int x2 = kVramWidth - 1;
    int y2 = kVramHeight - 1;
};

struct MaskSetting {        // GP0(E6h)
    bool set_bit = false;   // bit0: force bit15 on written pixels
    bool test_bit = false;  // bit1: skip pixels whose destination bit15 is set
};

// Everything texture_fetch needs.
struct TexEnv {
    DrawMode mode;
    TexWindow win;
    Clut clut;
    const Vram* vram = nullptr;  // set by the command processor
};

// Everything the blend / visibility stages need for the pixel in flight.
struct PrimCtx {
    int shade_r = 128;      // 8-bit primitive colour (80h = unity modulation)
    int shade_g = 128;
    int shade_b = 128;
    uint16_t shade15 = 0;   // primitive colour as 15-bit (untextured prims)
    bool textured = false;  // false for monochrome primitives
    bool raw = false;       // command bit24: raw-texture (decal), no modulation
    bool semi = false;      // command bit25: semi-transparent
    int semi_mode = 0;      // equation index 0-3 from GP0(E1h)/texpage bits 5-6
    bool dither = false;    // poly && texture-blended && DrawMode.dither
                            // (lines only in real hw; rects are NEVER dithered)
    MaskSetting mask{};
    DrawArea area{};
};

// ---------------------------------------------------------------------------
// Register-field decoding (infrastructure; unit-tested in 01_texture_pages).
// ---------------------------------------------------------------------------

inline int sext11(uint32_t v) {
    v &= 0x7FFu;
    return v >= 0x400 ? static_cast<int>(v) - 0x800 : static_cast<int>(v);
}

inline void decode_draw_mode(uint32_t w, DrawMode& m) {
    m.page_x_field = static_cast<int>(w & 0xF);
    m.page_y_base = (w & 0x10) ? 256 : 0;
    m.semi = static_cast<int>((w >> 5) & 3);
    m.depth = static_cast<int>((w >> 7) & 3);
    m.dither = (w >> 9) & 1;
}

inline void decode_tex_window(uint32_t w, TexWindow& t) {
    t.mask_x = static_cast<int>(w & 0x1F);
    t.mask_y = static_cast<int>((w >> 5) & 0x1F);
    t.off_x = static_cast<int>((w >> 10) & 0x1F);
    t.off_y = static_cast<int>((w >> 15) & 0x1F);
}

// Works for both GP0(E3h) and GP0(E4h): returns the encoded corner.
inline void decode_area_corner(uint32_t w, int& x, int& y) {
    x = static_cast<int>(w & 0x3FF);
    y = static_cast<int>((w >> 10) & 0x3FF);
}

inline void decode_draw_offset(uint32_t w, int& ox, int& oy) {
    ox = sext11(w);
    oy = sext11(w >> 11);
}

inline void decode_mask_setting(uint32_t w, MaskSetting& m) {
    m.set_bit = w & 1;
    m.test_bit = (w >> 1) & 1;
}

// CLUT attribute carried in the upper half of textured-primitive words:
// bits 0-5 = X in 16-halfword steps, bits 6-12 = Y.
inline Clut decode_clut(uint32_t attr_word) {
    Clut c;
    c.x = static_cast<int>((attr_word >> 16) & 0x3F) * 16;
    // Attribute bits 6-14 carry Y bits 0-8 (up to 511 lines).
    c.y = static_cast<int>((attr_word >> 22) & 0x1FF);
    return c;
}

// Texpage attribute (vertex-2 texcoord word of textured polygons) refreshes
// GP0(E1h) bits 0-8; dither (bit9) can only change via GP0(E1h) itself.
inline void merge_texpage_attr(uint32_t attr_word, DrawMode& m) {
    DrawMode tmp;
    decode_draw_mode(attr_word & 0x1FF, tmp);
    m.page_x_field = tmp.page_x_field;
    m.page_y_base = tmp.page_y_base;
    m.semi = tmp.semi;
    m.depth = tmp.depth;
}

// Vertex word: X bits 0-10 signed, Y bits 16-26 signed.
inline void decode_vertex(uint32_t w, int& x, int& y) {
    x = sext11(w);
    y = sext11(w >> 16);
}

// ---------------------------------------------------------------------------
// Colour helpers.
// ---------------------------------------------------------------------------

// Hardware expands 5-bit components to 8 bits by replication, not <<3:
// 31 -> 255 keeps FFh "brightest".
inline uint8_t expand5to8(uint8_t c) {
    return static_cast<uint8_t>((c << 3) | (c >> 2));
}

inline uint16_t rgb8_to_bgr15(int r, int g, int b) {
    return static_cast<uint16_t>(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
}

struct Rgb5 {
    int r, g, b;  // 5-bit components (red in bits 0-4)
};

inline Rgb5 unpack_bgr15(uint16_t c) {
    return {static_cast<int>(c & 0x1F), static_cast<int>((c >> 5) & 0x1F),
            static_cast<int>((c >> 10) & 0x1F)};
}

inline uint16_t pack_bgr15(int r, int g, int b) {
    auto sat = [](int v) { return v < 0 ? 0 : (v > 31 ? 31 : v); };
    return static_cast<uint16_t>((sat(b) << 10) | (sat(g) << 5) | sat(r));
}

// PSX-SPX "24bit RGB to 15bit RGB Dithering" table. Offsets are added to the
// 8-bit R/G/B values, the sum is saturated to 00h..FFh, then divided by 8.
constexpr int kDither[4][4] = {
    {-4, +0, -3, +1},
    {+2, -2, +3, -1},
    {-3, +1, -4, +0},
    {+3, -1, +2, -2},
};

}  // namespace psx::gpu
