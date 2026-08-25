#pragma once
// GBA sprites (OAM): attribute decoding, 1D/2D tile mapping, affine sprite
// matrices, and the sprite-vs-background priority rules.
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
constexpr u32 kObjTileBase = 0x10000; // OBJ character data offset inside VRAM
constexpr u32 kObjPalBase = 256;      // OBJ palette entries start at PAL[256]

struct PpuMemory {
    static constexpr u32 kIoBase = 0x04000000;

    u8 io[0x100] = {};
    u8 pal[0x400] = {};
    u8 vram[kVramSize] = {};
    u8 oam[0x400] = {};

    void reset() { *this = PpuMemory{}; }

    static u16 rd_le(const u8* p) { return u16(p[0]) | u16(p[1]) << 8; }
    static void wr_le(u8* p, u16 v) {
        p[0] = u8(v);
        p[1] = u8(v >> 8);
    }
};

// Sprite attributes decoded from one OAM slot (ATTR0..ATTR2).
struct ObjAttrs {
    int y = 0;
    bool affine = false;      // rotation/scaling flag
    bool double_size = false; // affine only; hides sprite when not affine
    int mode = 0;             // 0 normal, 1 semi-transparent, 2 OBJ window
    bool bpp8 = false;        // ATTR0 bit 13
    int shape = 0;            // 0 square, 1 wide, 2 tall
    int x = 0;                // 9 bits
    int matrix = 0;           // affine matrix select, 0-31
    bool hflip = false;       // non-affine only
    bool vflip = false;
    int size = 0;             // 0-3
    int tile = 0;
    int priority = 0;
    int bank = 0;

    int width() const {
        static constexpr int kW[3][4] = {{8, 16, 32, 64},
                                         {16, 32, 32, 64},
                                         {8, 8, 16, 32}};
        return kW[shape][size];
    }
    int height() const {
        static constexpr int kH[3][4] = {{8, 16, 32, 64},
                                         {8, 8, 16, 32},
                                         {16, 32, 32, 64}};
        return kH[shape][size];
    }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline ObjAttrs decode_obj_attrs(const PpuMemory& m, int slot) {
    const u8* o = m.oam + slot * 8;
    u16 a0 = u16(o[0]) | u16(o[1]) << 8;
    u16 a1 = u16(o[2]) | u16(o[3]) << 8;
    u16 a2 = u16(o[4]) | u16(o[5]) << 8;
    ObjAttrs s;
    s.y = a0 & 0xFF;
    s.affine = (a0 >> 8) & 1;
    s.double_size = (a0 >> 9) & 1;
    s.mode = (a0 >> 10) & 3;
    s.bpp8 = (a0 >> 13) & 1;
    s.shape = (a0 >> 14) & 3;
    s.x = a1 & 0x1FF;
    s.matrix = (a1 >> 9) & 31;
    s.hflip = !s.affine && ((a1 >> 12) & 1);
    s.vflip = !s.affine && ((a1 >> 13) & 1);
    s.size = (a1 >> 14) & 3;
    s.tile = a2 & 0x3FF;
    s.priority = (a2 >> 10) & 3;
    s.bank = (a2 >> 12) & 15;
    return s;
}
//@LABS-STUB
// TODO(1): decode the three attributes of an OAM slot (slot i lives at OAM
// bytes i*8 .. i*8+5). Key fields: ATTR0 y/affine/double/mode/256-color/
// shape, ATTR1 9-bit x/matrix/hflip/vflip/size, ATTR2 tile/priority/bank.
// Flips are ignored while the affine flag is set.
inline ObjAttrs decode_obj_attrs(const PpuMemory& m, int slot) {
    (void)m;
    (void)slot;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Byte offset of one pixel of a sprite, honoring 1D vs 2D mapping
// (DISPCNT bit 5). 1D packs rows linearly (tiles-per-row = width/8); 2D
// keeps the legacy layout of 32 tiles per row. 4bpp tiles are 32 bytes with
// low nibble = even column; 8bpp tiles are 64 bytes.
inline u32 sprite_pixel_byte(const ObjAttrs& s, bool mapping_1d, int col,
                             int row) {
    int tc = col / 8, tr = row / 8;
    int tiles_per_row = mapping_1d ? (s.width() / 8) : 32;
    u32 tile_bytes = s.bpp8 ? 64u : 32u;
    u32 row_bytes = s.bpp8 ? 8u : 4u;
    return kObjTileBase + u32(s.tile) * tile_bytes +
           u32(tr * tiles_per_row + tc) * tile_bytes +
           u32(row % 8) * row_bytes + u32(col % 8) / (s.bpp8 ? 1 : 2);
}
//@LABS-STUB
// TODO(2): compute the byte offset of a sprite pixel inside VRAM. Tiles per
// row is width/8 in 1D mapping but always 32 in 2D; tile size is 32 bytes
// (4bpp) or 64 bytes (8bpp); low nibble = even column in 4bpp.
inline u32 sprite_pixel_byte(const ObjAttrs& s, bool mapping_1d, int col,
                             int row) {
    (void)s;
    (void)mapping_1d;
    (void)col;
    (void)row;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Color palette index of one sprite pixel, or -1 when transparent. Handles
// h/vflip and the 4bpp bank (OBJ palettes live at entries 256-511).
inline int obj_pixel_index(const PpuMemory& m, const ObjAttrs& s,
                           bool mapping_1d, int col, int row) {
    int c = s.hflip ? s.width() - 1 - col : col;
    int r = s.vflip ? s.height() - 1 - row : row;
    u32 byte_addr = sprite_pixel_byte(s, mapping_1d, c, r) % kVramSize;
    if (!s.bpp8) {
        u8 byte = m.vram[byte_addr];
        int idx = (c & 1) ? (byte >> 4) : (byte & 0xF);
        return idx == 0 ? -1 : int(kObjPalBase) + s.bank * 16 + idx;
    }
    u8 idx = m.vram[byte_addr];
    return idx == 0 ? -1 : int(kObjPalBase) + idx;
}
//@LABS-STUB
// TODO(3): resolve one sprite pixel to a final palette index (-1 =
// transparent). Apply flips to the local coordinates first, then fetch via
// sprite_pixel_byte; in 4bpp combine the bank: index = 256 + bank*16 + n.
inline int obj_pixel_index(const PpuMemory& m, const ObjAttrs& s,
                           bool mapping_1d, int col, int row) {
    (void)m;
    (void)s;
    (void)mapping_1d;
    (void)col;
    (void)row;
    return -2;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Affine matrix coefficients live interleaved in OAM at byte 0x06 of every
// slot: matrix m's four s16 values sit at OAM offsets 0x06+m*32+{0,8,16,24}.
inline void load_obj_matrix(const PpuMemory& m, int matrix, s16& pa, s16& pb,
                            s16& pc, s16& pd) {
    const u8* base = m.oam + 0x06 + matrix * 32;
    auto rd = [&](int off) {
        return s16(u16(base[off]) | u16(base[off + 1]) << 8);
    };
    pa = rd(0);
    pb = rd(8);
    pc = rd(16);
    pd = rd(24);
}

// Map one affine-sprite pixel given a LOCAL coordinate inside the sprite's
// (possibly doubled) bounding box: centered, transformed through the inverse
// matrix in 8.8 fixed point, re-centered on the texture. Returns false when
// outside the texture (transparent).
inline bool affine_obj_texel(const ObjAttrs& s, const s16 pa, const s16 pb,
                             const s16 pc, const s16 pd, int px_on_screen,
                             int py_on_screen, int& tx, int& ty) {
    int w = s.width(), h = s.height();
    int half_w = s.double_size ? w : w / 2;
    int half_h = s.double_size ? h : h / 2;
    // Screen offsets are plain integers; multiplying by an 8.8 matrix
    // element keeps one fractional byte, removed by a single >>8.
    s32 rx = px_on_screen - half_w;
    s32 ry = py_on_screen - half_h;
    tx = int((s32(pa) * rx + s32(pb) * ry) >> 8) + w / 2;
    ty = int((s32(pc) * rx + s32(pd) * ry) >> 8) + h / 2;
    return unsigned(tx) < unsigned(w) && unsigned(ty) < unsigned(h);
}
//@LABS-STUB
// TODO(4): read an affine matrix from OAM (four s16 at byte 0x06 +
// matrix*32, spaced 8 bytes apart) and transform one screen pixel into
// texture space: center on the sprite, apply the matrix in 8.8 fixed point,
// add back half the texture size; return false when outside the texture.
inline void load_obj_matrix(const PpuMemory& m, int matrix, s16& pa, s16& pb,
                            s16& pc, s16& pd) {
    (void)m;
    (void)matrix;
    pa = pb = pc = pd = 0;  // wrong on purpose
}
inline bool affine_obj_texel(const ObjAttrs& s, const s16 pa, const s16 pb,
                             const s16 pc, const s16 pd, int px_on_screen,
                             int py_on_screen, int& tx, int& ty) {
    (void)s;
    (void)pa;
    (void)pb;
    (void)pc;
    (void)pd;
    (void)px_on_screen;
    (void)py_on_screen;
    tx = ty = -1;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Priority rule between one opaque sprite pixel and a background layer:
// lower priority value wins; EQUAL priority goes to the sprite. Returns the
// winning palette index (bg_index when the background wins, -1 for backdrop
// when neither shows... callers treat "no sprite pixel" separately).
inline int resolve_sprite_vs_bg(bool sprite_opaque, int sprite_priority,
                                int sprite_index, int bg_priority,
                                int bg_index) {
    (void)sprite_index;  // tie-break between sprites uses lowest OAM index
    if (!sprite_opaque) return bg_index;
    if (sprite_priority <= bg_priority) return sprite_index;
    return bg_index;
}
//@LABS-STUB
// TODO(5): implement the GBA priority rule: an opaque sprite beats any
// background whose priority value is >= the sprite's (equal -> sprite wins);
// a transparent sprite pixel lets the background show.
inline int resolve_sprite_vs_bg(bool sprite_opaque, int sprite_priority,
                                int sprite_index, int bg_priority,
                                int bg_index) {
    (void)sprite_opaque;
    (void)sprite_priority;
    (void)sprite_index;
    (void)bg_priority;
    (void)bg_index;
    return -2;  // wrong on purpose
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
