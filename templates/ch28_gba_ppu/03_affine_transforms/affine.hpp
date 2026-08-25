#pragma once
// GBA affine backgrounds: 8.8 fixed-point transform math, reference points,
// per-scanline latched counters and wrapping texel fetch (mode 2 BG2/BG3,
// mode 1 BG2).
//
// Model note: PA..PD and the reference point use 8 fractional bits here
// (`coord >> 8` = texel). Hardware keeps extra sub-texel bits internally,
// which cannot change any rendered texel, so we omit them.
#include <cstdint>
#include <vector>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr int kScreenW = 240;
constexpr int kScreenH = 160;
constexpr u32 kVramSize = 0x18000;

struct PpuMemory {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kPalBase = 0x05000000;
    static constexpr u32 kVramBase = 0x06000000;

    u8 io[0x100] = {};
    u8 pal[0x400] = {};
    u8 vram[kVramSize] = {};

    void reset() { *this = PpuMemory{}; }

    static u16 rd_le(const u8* p) { return u16(p[0]) | u16(p[1]) << 8; }
    static void wr_le(u8* p, u16 v) {
        p[0] = u8(v);
        p[1] = u8(v >> 8);
    }

    u16 rd16(u32 addr) const {
        switch ((addr >> 24) & 7) {
            case 4: return rd_le(io + ((addr - kIoBase) & 0xFE));
            case 6:
                return rd_le(vram + (((addr - kVramBase) % kVramSize) &
                                     ~1u));
            default: return 0;
        }
    }
    void wr16(u32 addr, u16 v) {
        switch ((addr >> 24) & 7) {
            case 4: wr_le(io + ((addr - kIoBase) & 0xFE), v); break;
            case 5: wr_le(pal + (addr & 0x3FE), v); break;
            case 6:
                wr_le(vram + (((addr - kVramBase) % kVramSize) & ~1u), v);
                break;
            default: break;
        }
    }
    u16 dispcnt() const { return rd16(kIoBase); }

    // Affine parameter block: BG2 at IO+0x20, BG3 at IO+0x30. Index 0..3 =
    // PA/PB/PC/PD, 4/5 = DX/DY halves (low, high).
    u16 affine_reg(int bg, int index) const {
        u32 base = (bg == 3 ? 0x30u : 0x20u) + u32(index) * 2;
        return rd16(kIoBase + base);
    }
};

// Affine parameter set; DX/DY hold the texture coordinate of screen pixel
// (0,0). Defaults describe the identity transform.
struct AffineParams {
    s16 pa = 256, pb = 0, pc = 0, pd = 256;
    s32 dx = 0, dy = 0;

    static AffineParams decode(const PpuMemory& m, int bg);
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Multiply two 8.8 fixed-point values keeping 8 fractional bits.
inline s32 fixed_mul(s32 a, s32 b) { return (a * b) >> 8; }

inline AffineParams AffineParams::decode(const PpuMemory& m, int bg) {
        AffineParams p;
        p.pa = s16(m.affine_reg(bg, 0));
        p.pb = s16(m.affine_reg(bg, 1));
        p.pc = s16(m.affine_reg(bg, 2));
        p.pd = s16(m.affine_reg(bg, 3));
        // Reference point registers are write-only on hardware; an emulator
        // naturally keeps shadow copies, which our flat IO array provides.
        auto rd32 = [&](int idx) {
            return s32(m.affine_reg(bg, idx)) |
                   s32(m.affine_reg(bg, idx + 1)) << 16;
        };
        p.dx = rd32(4);
        p.dy = rd32(6);
        return p;
}
//@LABS-STUB
// TODO(1): implement 8.8 fixed multiply ((a*b) >> 8 on s32) and decode the
// affine parameter block (PA/PB/PC/PD as s16 at IO+0x20/+2/+4/+6, reference
// point DX/DY as u32 pairs at +8 and +12; BG3 uses IO+0x30 instead of 0x20).
inline s32 fixed_mul(s32 a, s32 b) {
    (void)a;
    (void)b;
    return 0;  // wrong on purpose
}
inline AffineParams AffineParams::decode(const PpuMemory&, int) {
    return {};  // wrong on purpose: drops the identity matrix too
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Per-scanline latch: the hardware recomputes its internal counters from the
// written reference point every line, adding PB/PD scaled by the line
// number. Everything stays in 8.8 fixed point (matrix element times scalar
// keeps its fraction); the single >>8 down to plain texels happens at fetch.
inline void latch_line(const AffineParams& p, int line, s32& out_x,
                       s32& out_y) {
    out_x = p.dx + s32(p.pb) * line;
    out_y = p.dy + s32(p.pd) * line;
}
//@LABS-STUB
// TODO(2): latch internal counters for a scanline:
// out_x = dx + pb * line, out_y = dy + pd * line (8.8 fixed throughout).
inline void latch_line(const AffineParams& p, int line, s32& out_x,
                       s32& out_y) {
    (void)p;
    (void)line;
    out_x = -1;  // wrong on purpose
    out_y = -1;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Texture coordinate of screen column x on a scanline latched at (sx, sy):
// horizontal step is PA per pixel, vertical step is PC (still 8.8 fixed;
// shift down by 8 only when fetching a texel).
inline void texel_coord(s32 sx, s32 sy, const AffineParams& p, int x,
                        s32& out_tx, s32& out_ty) {
    out_tx = sx + s32(p.pa) * x;
    out_ty = sy + s32(p.pc) * x;
}
//@LABS-STUB
// TODO(3): advance to screen column x: tx = sx + pa * x,
// ty = sy + pc * x (all values remain 8.8 fixed point).
inline void texel_coord(s32 sx, s32 sy, const AffineParams& p, int x,
                        s32& out_tx, s32& out_ty) {
    (void)sx;
    (void)sy;
    (void)p;
    (void)x;
    out_tx = 0;  // wrong on purpose
    out_ty = 0;
}
//@LABS-END

// Affine background geometry: square textures of 128/256/512/1024 texels.
// Screen map entries are SINGLE BYTES (tile index 0-255); tiles are 64-byte
// 8bpp. Coordinates wrap modulo the texture size — that is why affine
// backgrounds repeat seamlessly forever.
struct AffineBgConfig {
    int priority = 0;
    int char_base = 0;    // byte offset, 16 KiB units
    int screen_base = 0;  // byte offset, 2 KiB units
    int size_log2 = 7;    // texture edge = 1 << size_log2 texels

    int texels() const { return 1 << size_log2; }
};

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline AffineBgConfig decode_affine_bg_config(u16 cnt) {
    AffineBgConfig c;
    c.priority = cnt & 3;
    c.char_base = ((cnt >> 2) & 3) * 0x4000;
    c.screen_base = ((cnt >> 8) & 31) * 0x800;
    c.size_log2 = 7 + ((cnt >> 14) & 3);
    return c;
}

inline int affine_texel(const PpuMemory& m, const AffineBgConfig& cfg,
                        s32 tx, s32 ty) {
    int edge = cfg.texels();
    int px = int(u32(tx) & u32(edge - 1));
    int py = int(u32(ty) & u32(edge - 1));
    int tiles_per_row = edge / 8;
    u8 entry =
        m.vram[(u32(cfg.screen_base) + u32(py / 8) * tiles_per_row +
                u32(px / 8)) %
               kVramSize];
    u32 tile_base = u32(cfg.char_base) + u32(entry) * 64;
    u8 color =
        m.vram[(tile_base + u32(py % 8) * 8 + u32(px % 8)) % kVramSize];
    return color == 0 ? -1 : int(color);
}
//@LABS-STUB
// TODO(4): fetch a wrapped affine texel. Mask tx/ty to the texture edge
// (power of two), read the single-BYTE screen entry at
// screen_base + (py/8)*tiles_per_row + px/8, then the 64-byte 8bpp tile;
// color 0 is transparent (-1).
inline int affine_texel(const PpuMemory& m, const AffineBgConfig& cfg,
                        s32 tx, s32 ty) {
    (void)m;
    (void)cfg;
    (void)tx;
    (void)ty;
    return -2;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Render one scanline of an affine background into palette indices (-1 =
// backdrop).
inline void render_affine_scanline(const PpuMemory& m,
                                   const AffineBgConfig& cfg,
                                   const AffineParams& p, int line,
                                   int out_indices[kScreenW]) {
    s32 sx, sy;
    latch_line(p, line, sx, sy);
    for (int x = 0; x < kScreenW; ++x) {
        s32 tx, ty;
        texel_coord(sx, sy, p, x, tx, ty);
        out_indices[x] = affine_texel(m, cfg, tx >> 8, ty >> 8);
    }
}
//@LABS-STUB
// TODO(5): latch the scanline's internal counters, then walk the 240 columns
// advancing through texel_coord and store each wrapped texel index.
inline void render_affine_scanline(const PpuMemory& m,
                                   const AffineBgConfig& cfg,
                                   const AffineParams& p, int line,
                                   int out_indices[kScreenW]) {
    (void)m;
    (void)cfg;
    (void)p;
    (void)line;
    for (int x = 0; x < kScreenW; ++x) out_indices[x] = -3;  // TODO(5)
}
//@LABS-END

inline u64 fnv64(const void* data, size_t n) {
    u64 h = 0xCBF29CE484222325ull;
    const u8* p = static_cast<const u8*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace gba
