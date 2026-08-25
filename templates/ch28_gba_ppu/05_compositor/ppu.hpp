#pragma once
// GBA scanline compositor: unifies bitmap modes, text backgrounds, affine
// backgrounds and sprites into one prioritized per-pixel pipeline, then
// applies windows, mosaic and color-math special effects.
//
// Layer IDs used throughout: 0-3 = BG0-BG3, 4 = OBJ, 5 = backdrop.
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
constexpr u32 kObjTileBase = 0x10000;
constexpr u32 kObjPalBase = 256;

struct PpuMemory {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kPalBase = 0x05000000;
    static constexpr u32 kVramBase = 0x06000000;

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

    void wr8_vram(u32 byte_offset, u8 v) { vram[byte_offset % kVramSize] = v; }

    u16 rd16(u32 addr) const {
        switch ((addr >> 24) & 7) {
            case 4: return rd_le(io + ((addr - kIoBase) & 0xFE));
            case 5: return rd_le(pal + (addr & 0x3FE));
            case 6:
                return rd_le(vram + (((addr - 0x06000000) % kVramSize) &
                                     ~1u));
            case 7: return rd_le(oam + (addr & 0x3FE));
            default: return 0;
        }
    }
    void wr16(u32 addr, u16 v) {
        switch ((addr >> 24) & 7) {
            case 4: wr_le(io + ((addr - kIoBase) & 0xFE), v); break;
            case 5: wr_le(pal + (addr & 0x3FE), v); break;
            case 6:
                wr_le(vram + (((addr - 0x06000000) % kVramSize) & ~1u), v);
                break;
            case 7: wr_le(oam + (addr & 0x3FE), v); break;
            default: break;
        }
    }
    u16 io16(int offset) const { return rd_le(io + offset); }
};

// ---------------------------------------------------------------------------
// Provided plumbing (conversion, hashing, per-subsystem fetch primitives
// already built in chapters 01-04).
// ---------------------------------------------------------------------------

inline u32 bgr555_to_rgba8888(u16 c) {
    u32 r5 = c & 31u, g5 = (c >> 5) & 31u, b5 = (c >> 10) & 31u;
    u32 r = (r5 << 3) | (r5 >> 2);
    u32 g = (g5 << 3) | (g5 >> 2);
    u32 b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

inline u64 fnv64(const void* data, size_t n) {
    u64 h = 0xCBF29CE484222325ull;
    const u8* p = static_cast<const u8*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

struct DispInfo {
    int mode = 0;
    bool obj_1d = false;
    bool forced_blank = false;
    bool obj_en = false;
    bool bg_en[4] = {false, false, false, false};
};

inline DispInfo decode_disp(u16 dc) {
    DispInfo d;
    d.mode = dc & 7;
    d.obj_1d = (dc >> 5) & 1;
    d.forced_blank = (dc >> 6) & 1;
    d.obj_en = (dc >> 7) & 1;
    for (int i = 0; i < 4; ++i) d.bg_en[i] = (dc >> (8 + i)) & 1;
    return d;
}

// --- text backgrounds ------------------------------------------------------
struct TextBgConfig {
    int priority = 0;
    int char_base = 0;
    bool bpp8 = false;
    int screen_base = 0;
    int map_w_tiles = 32;
    int map_h_tiles = 32;
};

inline TextBgConfig decode_text_bg_config(u16 cnt) {
    static constexpr int kW[] = {32, 64, 32, 64};
    static constexpr int kH[] = {32, 32, 64, 64};
    TextBgConfig c;
    c.priority = cnt & 3;
    c.char_base = ((cnt >> 2) & 3) * 0x4000;
    c.bpp8 = (cnt >> 7) & 1;
    c.screen_base = ((cnt >> 8) & 31) * 0x800;
    int size = (cnt >> 14) & 3;
    c.map_w_tiles = kW[size];
    c.map_h_tiles = kH[size];
    return c;
}

inline int text_bg_pixel_at(const PpuMemory& m, const TextBgConfig& cfg,
                            u32 hofs, u32 vofs, int x, int y) {
    int px = (x + int(hofs)) % (cfg.map_w_tiles * 8);
    int py = (y + int(vofs)) % (cfg.map_h_tiles * 8);
    int tx = px / 8, ty = py / 8;
    u32 block = u32(tx / 32) + u32(ty / 32) * 32;
    u16 raw =
        m.rd16(0x06000000 + u32(cfg.screen_base) + block * 0x800 +
               u32(ty % 32) * 64 + u32(tx % 32) * 2);
    int tile = raw & 0x3FF;
    bool hf = (raw >> 10) & 1, vf = (raw >> 11) & 1;
    int bank = (raw >> 12) & 0xF;
    int lx = hf ? 7 - px % 8 : px % 8;
    int ly = vf ? 7 - py % 8 : py % 8;
    u32 tb = u32(cfg.char_base) + u32(tile) * (cfg.bpp8 ? 64u : 32u);
    int idx;
    if (!cfg.bpp8) {
        u8 byte = m.vram[(tb + u32(ly) * 4 + u32(lx >> 1)) % kVramSize];
        int n = (lx & 1) ? (byte >> 4) : (byte & 0xF);
        idx = n == 0 ? -1 : bank * 16 + n;
    } else {
        u8 n = m.vram[(tb + u32(ly) * 8 + u32(lx)) % kVramSize];
        idx = n == 0 ? -1 : int(n);
    }
    return idx;
}

// --- affine backgrounds ----------------------------------------------------
struct AffineParams {
    s16 pa = 256, pb = 0, pc = 0, pd = 256;
    s32 dx = 0, dy = 0;
};

inline AffineParams load_affine_params(const PpuMemory& m, int bg) {
    u32 base = (bg == 3 ? 0x30u : 0x20u);
    AffineParams p;
    p.pa = s16(m.io16(int(base)));
    p.pb = s16(m.io16(int(base + 2)));
    p.pc = s16(m.io16(int(base + 4)));
    p.pd = s16(m.io16(int(base + 6)));
    auto rd32 = [&](int off) {
        return s32(m.io16(off)) | s32(m.io16(off + 2)) << 16;
    };
    p.dx = rd32(int(base + 8));
    p.dy = rd32(int(base + 12));
    return p;
}

struct AffineBgConfig {
    int priority = 0;
    int char_base = 0;
    int screen_base = 0;
    int size_log2 = 7;
    int texels() const { return 1 << size_log2; }
};

inline AffineBgConfig decode_affine_bg_config(u16 cnt) {
    AffineBgConfig c;
    c.priority = cnt & 3;
    c.char_base = ((cnt >> 2) & 3) * 0x4000;
    c.screen_base = ((cnt >> 8) & 31) * 0x800;
    c.size_log2 = 7 + ((cnt >> 14) & 3);
    return c;
}

inline int affine_texel_at(const PpuMemory& m, const AffineBgConfig& cfg,
                           s32 tx, s32 ty) {
    int edge = cfg.texels();
    int px = int(u32(tx) & u32(edge - 1));
    int py = int(u32(ty) & u32(edge - 1));
    int tpr = edge / 8;
    u8 e = m.vram[(u32(cfg.screen_base) + u32(py / 8) * tpr + u32(px / 8)) %
                  kVramSize];
    u8 c = m.vram[(u32(cfg.char_base) + u32(e) * 64 + u32(py % 8) * 8 +
                   u32(px % 8)) %
                  kVramSize];
    return c == 0 ? -1 : int(c);
}

// --- sprites ---------------------------------------------------------------
struct ObjAttrs {
    int y = 0;
    bool affine = false;
    bool double_size = false;
    int mode = 0;
    bool bpp8 = false;
    int shape = 0;
    int x = 0;
    int matrix = 0;
    bool hflip = false;
    bool vflip = false;
    int size = 0;
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

inline int obj_pixel_index(const PpuMemory& m, const ObjAttrs& s, bool map1d,
                           int col, int row) {
    int c = s.hflip ? s.width() - 1 - col : col;
    int r = s.vflip ? s.height() - 1 - row : row;
    int tc = c / 8, tr = r / 8;
    int tpr = map1d ? (s.width() / 8) : 32;
    u32 tile_bytes = s.bpp8 ? 64u : 32u;
    u32 row_bytes = s.bpp8 ? 8u : 4u;
    u32 addr = kObjTileBase + u32(s.tile) * tile_bytes +
               u32(tr * tpr + tc) * tile_bytes + u32(r % 8) * row_bytes +
               u32(c % 8) / (s.bpp8 ? 1u : 2u);
    if (!s.bpp8) {
        u8 byte = m.vram[addr % kVramSize];
        int n = (c & 1) ? (byte >> 4) : (byte & 0xF);
        return n == 0 ? -1 : int(kObjPalBase) + s.bank * 16 + n;
    }
    u8 n = m.vram[addr % kVramSize];
    return n == 0 ? -1 : int(kObjPalBase) + n;
}

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

// ---------------------------------------------------------------------------
// Student tasks.
// ---------------------------------------------------------------------------

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Visibility mask for one pixel: bits 0-3 BG0-BG3, 4 OBJ, 5 blend-enable.
// Inside WIN0 uses WININ low byte, else inside WIN1 uses WININ high byte,
// else WINOUT low byte. Rects are half-open; x1 > x2 disables a window.
// (The OBJ window, DISPCNT bit 15, is documented but not implemented here.)
inline u8 window_mask(const PpuMemory& m, int x, int y) {
    auto inside = [&](u16 hv, u16 vv) {
        int x1 = hv & 0xFF, x2 = (hv >> 8) & 0xFF;
        int y1 = vv & 0xFF, y2 = (vv >> 8) & 0xFF;
        if (x1 > x2 || y1 > y2) return false;
        return x >= x1 && x < x2 && y >= y1 && y < y2;
    };
    // WIN0H/V at 0x40/0x42, WIN1H/V at 0x44/0x46, WININ 0x48, WINOUT 0x4A.
    if (inside(m.io16(0x40), m.io16(0x42))) return u8(m.io16(0x48) & 0x3F);
    if (inside(m.io16(0x44), m.io16(0x46)))
        return u8((m.io16(0x48) >> 8) & 0x3F);
    return u8(m.io16(0x4A) & 0x3F);
}
//@LABS-STUB
// TODO(1): compute the 6-bit window visibility mask for one pixel. Rect
// registers hold start byte / end byte pairs: WIN0H=IO 0x40, WIN0V=0x42,
// WIN1H=0x44, WIN1V=0x46. Masks live in WININ (0x48) low/high bytes and
// WINOUT (0x4A) low byte for the area outside both windows. Ranges are
// half-open; an inverted rect disables its window.
inline u8 window_mask(const PpuMemory& m, int x, int y) {
    (void)m;
    (void)x;
    (void)y;
    return 0x3F;  // wrong on purpose: ignores windows entirely
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Mosaic quantization: MOSAIC registers hold block-size-minus-one. All
// pixels of a block sample the background at the block's origin.
inline void mosaic_quantize(int& x, int& y, u8 w_minus1, u8 h_minus1) {
    int bw = int(w_minus1) + 1;
    int bh = int(h_minus1) + 1;
    x -= x % bw;
    y -= y % bh;
}
//@LABS-STUB
// TODO(2): snap x/y down to their mosaic block origins. Register values are
// block-size-minus-one (0 disables that axis because the block becomes 1).
inline void mosaic_quantize(int& x, int& y, u8 w_minus1, u8 h_minus1) {
    (void)x;
    (void)y;
    (void)w_minus1;
    (void)h_minus1;  // TODO(2): wrong on purpose
}
//@LABS-END

// One resolved pixel of a layer.
struct LayerPix {
    bool opaque = false;
    u16 color = 0;  // BGR555
    int priority = 3;
    bool semi = false;   // OBJ semi-transparent (mode 1)
    int layer_id = -1;   // 0-3 BGn, 4 OBJ, 5 backdrop (BLDCNT targeting)
};

inline u16 pal_color(const PpuMemory& m, int index) {
    return m.rd16(0x05000000 + u32(index) * 2);
}

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Resolve background `id` at (x,y) honoring mode geometry, enables, mosaic
// and scroll. Bitmap layers live on BG2 (modes 3/4/5); text BGs exist in
// modes 0/1; affine BG2/BG3 in modes 1/2. Index 0 pixels are transparent.
inline LayerPix bg_layer_pixel(const PpuMemory& m, const DispInfo& d, int id,
                               int x, int y) {
    LayerPix p;
    if (!d.bg_en[id]) return p;
    u16 cnt = m.io16(8 + id * 2);
    u32 mos = m.rd16(0x0400004C);
    int qx = x, qy = y;
    // BG mosaic applies only to character/background layers.
    if (d.mode <= 2 && (cnt >> 6) & 1)
        mosaic_quantize(qx, qy, u8(mos & 0xF), u8((mos >> 4) & 0xF));

    auto finish_idx = [&](int idx) {
        if (idx < 0) return p;
        p.opaque = true;
        p.layer_id = id;
        p.priority = cnt & 3;
        p.color = pal_color(m, idx);
        return p;
    };

    if (d.mode == 3 || d.mode == 5) {
        if (id != 2) return p;
        u16 page = d.mode == 5 && (m.io16(0) & 0x10) ? 0xA000u : 0u;
        int w = d.mode == 5 ? 160 : 240;
        if (x >= w || y >= (d.mode == 5 ? 128 : 160)) return p;
        p.opaque = true;
        p.layer_id = id;
        p.priority = cnt & 3;
        p.color =
            m.rd16(0x06000000 + page + (u32(y) * u32(w) + u32(x)) * 2);
        return p;
    }
    if (d.mode == 4) {
        if (id != 2) return p;
        u32 page = (m.io16(0) & 0x10) ? 0xA000u : 0u;
        u32 byte = page + u32(y) * 240 + u32(x);
        u8 index = byte & 1
                       ? u8(m.rd16(0x06000000 + (byte & ~1u)) >> 8)
                       : u8(m.rd16(0x06000000 + (byte & ~1u)));
        return finish_idx(index == 0 ? -1 : int(index));
    }
    if (d.mode == 1 && id == 2) {
        AffineParams ap = load_affine_params(m, 2);
        AffineBgConfig cfg = decode_affine_bg_config(cnt);
        s32 sx = ap.dx + s32(ap.pb) * y, sy = ap.dy + s32(ap.pd) * y;
        s32 tx = sx + s32(ap.pa) * qx, ty = sy + s32(ap.pc) * qx;
        return finish_idx(affine_texel_at(m, cfg, tx >> 8, ty >> 8));
    }
    if (d.mode == 2 && (id == 2 || id == 3)) {
        AffineParams ap = load_affine_params(m, id);
        AffineBgConfig cfg = decode_affine_bg_config(cnt);
        s32 sx = ap.dx + s32(ap.pb) * y, sy = ap.dy + s32(ap.pd) * y;
        s32 tx = sx + s32(ap.pa) * qx, ty = sy + s32(ap.pc) * qx;
        return finish_idx(affine_texel_at(m, cfg, tx >> 8, ty >> 8));
    }
    if (d.mode <= 1) {
        TextBgConfig cfg = decode_text_bg_config(cnt);
        u32 hofs = m.io16(int(0x10 + id * 4)) & 0x1FF;
        u32 vofs = m.io16(int(0x10 + id * 4 + 2)) & 0x1FF;
        return finish_idx(text_bg_pixel_at(m, cfg, hofs, vofs, qx, qy));
    }
    return p;
}
//@LABS-STUB
// TODO(3): resolve one background pixel into a LayerPix (opaque + BGR555
// color + priority). Dispatch on mode: 3/5 direct bitmap on BG2, 4 paletted
// (index 0 transparent), 1/2 affine via latched counters, 0/1 text via
// masked scroll. Respect DISPCNT enable bits; apply BG mosaic to text layers.
inline LayerPix bg_layer_pixel(const PpuMemory& m, const DispInfo& d, int id,
                               int x, int y) {
    (void)m;
    (void)d;
    (void)id;
    (void)x;
    (void)y;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Best sprite pixel covering (x,y): visible sprites are scanned by priority
// value, ties broken by the LOWEST OAM index. Sprites scrolled past y=255
// wrap; OBJ-window-mode sprites do not render (documented simplification).
inline LayerPix obj_layer_pixel(const PpuMemory& m, const DispInfo& d, int x,
                                int y) {
    LayerPix best;
    if (!d.obj_en) return best;
    for (int pass = 0; pass < 4; ++pass) {
        for (int slot = 127; slot >= 0; --slot) {
            ObjAttrs s = decode_obj_attrs(m, slot);
            if (s.priority != pass) continue;
            if (s.mode == 2) continue;  // OBJ window mask, not drawn here
            if (!s.affine && s.double_size) continue;  // hidden
            int w = s.width(), h = s.height();
            if (s.affine && s.double_size) {
                w *= 2;
                h *= 2;
            }
            int row = (y - s.y + 256) % 256;
            int col = (x - s.x + 512) % 512;
            if (row >= h || col >= w) continue;
            int index;
            if (s.affine) {
                s16 pa, pb, pc, pd;
                load_obj_matrix(m, s.matrix, pa, pb, pc, pd);
                int cx = s.double_size ? w / 2 : w / 2;
                int cy = s.double_size ? h / 2 : h / 2;
                int tx = int((s32(pa) * (col - cx) +
                              s32(pb) * (row - cy)) >> 8) +
                         s.width() / 2;
                int ty = int((s32(pc) * (col - cx) +
                              s32(pd) * (row - cy)) >> 8) +
                         s.height() / 2;
                if (unsigned(tx) >= unsigned(s.width()) ||
                    unsigned(ty) >= unsigned(s.height()))
                    continue;
                index = obj_pixel_index(m, s, d.obj_1d, tx, ty);
            } else {
                index = obj_pixel_index(m, s, d.obj_1d, col, row);
            }
            if (index < 0) continue;
            // Later slots lose to earlier ones at equal priority, so keep
            // overwriting while iterating downward, then stop.
            best.opaque = true;
            best.layer_id = 4;
            best.priority = pass;
            best.semi = s.mode == 1;
            best.color = pal_color(m, index);
        }
        if (best.opaque) break;
    }
    return best;
}
//@LABS-STUB
// TODO(4): find the best sprite pixel covering (x,y). Scan OAM slots by
// priority value; equal priorities favor the LOWER slot number. Wrap-around
// positioning: row=(y-s.y+256)%256, col=(x-s.x+512)%512. Honor affine
// matrices, double-size ranges, flips, and hide non-affine double-size
// entries. Transparent pixels never occlude.
inline LayerPix obj_layer_pixel(const PpuMemory& m, const DispInfo& d, int x,
                                int y) {
    (void)m;
    (void)d;
    (void)x;
    (void)y;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Compositing order: priority value first; OBJ beats BGs at equal priority;
// BGs at equal priority favor the lower id; backdrop sits under everything.
// Produces the top layer and the layer directly beneath it (for blending).
inline void pick_layers(const LayerPix (&bgs)[4], const LayerPix& obj,
                        LayerPix& top, LayerPix& second) {
    top = LayerPix{};
    second = LayerPix{};
    auto consider = [](const LayerPix& cand, LayerPix& top, LayerPix& second) {
        if (!cand.opaque) return;
        if (!top.opaque) {
            top = cand;
        } else if (!second.opaque) {
            second = cand;
        }
    };
    for (int pr = 0; pr < 4; ++pr) {
        consider(obj.priority == pr ? obj : LayerPix{}, top, second);
        for (int id = 0; id < 4; ++id)
            if (bgs[id].opaque && bgs[id].priority == pr)
                consider(bgs[id], top, second);
    }
}
//@LABS-STUB
// TODO(5): order the four background pixels plus the sprite pixel into the
// compositing sequence (priority value asc; OBJ wins ties vs BG; lower BG id
// wins ties vs BG; skip transparent pixels) and output the first two opaque
// layers as top/second.
inline void pick_layers(const LayerPix (&bgs)[4], const LayerPix& obj,
                        LayerPix& top, LayerPix& second) {
    (void)bgs;
    (void)obj;
    top = LayerPix{};
    second = LayerPix{};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
// Color special effects (BLDCNT/BLDALPHA/BLDY). Alpha blend:
// out = min(31, (top*EVA + bottom*EVB) >> 4), coefficients clamped to 16
// because hardware saturates anything above. A semi-transparent sprite
// forces the alpha path against whatever BLDCNT marks as 2nd targets.
inline u16 apply_bld(const PpuMemory& m, const LayerPix& top,
                     const LayerPix& second, bool blend_allowed) {
    u16 bldcnt = m.io16(0x50);
    u16 bldalpha = m.io16(0x52);
    u16 bldy = m.io16(0x54);
    int mode = (bldcnt >> 6) & 3;

    auto chan = [](u16 c, int s) -> u32 { return (c >> s) & 31u; };
    auto pack = [](u32 r, u32 g, u32 b) {
        return u16(r | (g << 5) | (b << 10));
    };

    bool first_target =
        top.opaque && ((((bldcnt >> top.layer_id) & 1) != 0) || top.semi);
    bool second_target =
        second.opaque && ((bldcnt >> (second.layer_id + 8)) & 1) != 0;

    if ((mode == 1 || top.semi) && first_target && second_target &&
        blend_allowed) {
        u32 eva = bldalpha & 0x1F;
        u32 evb = (bldalpha >> 8) & 0x1F;
        if (eva > 16) eva = 16;
        if (evb > 16) evb = 16;
        return pack(
            (chan(top.color, 0) * eva + chan(second.color, 0) * evb) >> 4,
            (chan(top.color, 5) * eva + chan(second.color, 5) * evb) >> 4,
            (chan(top.color, 10) * eva + chan(second.color, 10) * evb) >> 4);
    }
    if (blend_allowed && first_target && mode == 2) {
        u32 ey = bldy & 0x1F;
        if (ey > 16) ey = 16;
        auto up = [&](int s) {
            u32 c = chan(top.color, s);
            return c + (31 - c) * ey / 16;
        };
        return pack(up(0), up(5), up(10));
    }
    if (blend_allowed && first_target && mode == 3) {
        u32 ey = bldy & 0x1F;
        if (ey > 16) ey = 16;
        auto down = [&](int s) {
            u32 c = chan(top.color, s);
            return c - c * ey / 16;
        };
        return pack(down(0), down(5), down(10));
    }
    return top.color;
}
//@LABS-STUB
// TODO(6): implement color effects. Read BLDCNT (first-target bits 0-5,
// effect mode bits 6-7, second-target bits 8-13), BLDALPHA (EVA bits 0-4,
// EVB bits 8-12, clamp to 16) and BLDY (bits 0-4). Mode 1 alpha-blends top
// over second; modes 2/3 lighten/darken toward white/black; semi-transparent
// sprites always alpha-blend. Without an effect return the top color.
inline u16 apply_bld(const PpuMemory& m, const LayerPix& top,
                     const LayerPix& second, bool blend_allowed) {
    (void)m;
    (void)top;
    (void)second;
    (void)blend_allowed;
    return 0x7FFF;  // wrong on purpose (white)
}
//@LABS-END

// ---------------------------------------------------------------------------
// Full-frame composition (provided glue using everything above).
// ---------------------------------------------------------------------------

inline void compose_frame(const PpuMemory& m, u32* out) {
    DispInfo d = decode_disp(m.io16(0));
    for (int y = 0; y < kScreenH; ++y) {
        for (int x = 0; x < kScreenW; ++x) {
            u32& dst = out[u32(y) * kScreenW + u32(x)];
            if (d.forced_blank) {
                dst = 0xFFFFFFFFu;
                continue;
            }
            u8 mask = window_mask(m, x, y);
            LayerPix bgs[4];
            for (int id = 0; id < 4; ++id) {
                if (!((mask >> id) & 1)) continue;
                bgs[id] = bg_layer_pixel(m, d, id, x, y);
            }
            LayerPix obj;
            if ((mask >> 4) & 1) obj = obj_layer_pixel(m, d, x, y);
            LayerPix top, second;
            pick_layers(bgs, obj, top, second);
            // The backdrop sits under everything and is always visible.
            LayerPix backdrop;
            backdrop.opaque = true;
            backdrop.layer_id = 5;
            backdrop.color = pal_color(m, 0);
            if (!top.opaque) {
                top = backdrop;
            } else if (!second.opaque) {
                second = backdrop;
            }
            dst = bgr555_to_rgba8888(
                apply_bld(m, top, second, ((mask >> 5) & 1) != 0));
        }
    }
}

}  // namespace gba
