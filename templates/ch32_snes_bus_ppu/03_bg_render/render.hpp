#pragma once
// Exercise 03 — background rendering: modes 0 and 1, windows, color math.
//
// Pipeline per output pixel (x, y):
//
//   1. Each active layer samples its 32x32-tile tilemap (scroll offsets wrap
//      modulo 256 pixels — one screen; larger map sizes are out of scope).
//   2. The sampled tilemap entry selects a tile; the tile's planar pixel is
//      fetched (2bpp or 4bpp), honouring h/v flip.
//   3. Color index 0 is transparent: the candidate is discarded so lower
//      layers (or the backdrop, CGRAM entry 0) show through.
//   4. If a window is enabled, a layer's candidate survives only when the
//      pixel lies in the effective window AND the layer's mask bit is set.
//   5. compose() picks the winner. DOCUMENTED RULE (our exact
//      simplification): sprites are omitted entirely; the winner minimizes
//      layer first and then maximizes the priority bit — smallest value of
//      key (layer * 2 + (priority ? 0 : 1)) wins. So BG1 always outranks
//      BG2/BG3 regardless of their priority bits, and within one layer
//      priority=1 outranks priority=0.
//   6. Color math (optional) combines the winner with the backdrop in the
//      5-bit-per-channel BGR555 domain with saturating clamp.
//   7. The final BGR555 entry expands to RGB888 with 5->8 bit replication
//      and is stored as RGBA (alpha $FF).
//
// Palette bands (documented deviation from real hardware, which shares
// CGRAM between layers):
//   Mode 1: BG1/BG2 (4bpp) use entries palette*16 + color (palette 0-7).
//           BG3 (2bpp) uses the dedicated band 32 + palette*4 + color.
//   Mode 0: layer n (2bpp) uses band n*32 + palette*4 + color (n = 0..3),
//           i.e. BG1 owns entries 0-31, BG2 32-63, BG3 64-95, BG4 96-127.
//
// Tile layouts (standard SNES planar formats):
//   2bpp: 8 bytes/tile. Row r: byte 2r = plane 0, byte 2r+1 = plane 1.
//         pixel = bit(7-col) of plane0 | bit(7-col) of plane1 << 1.
//   4bpp: 32 bytes/tile. Rows 0-15 read plane 0/1 pairs at bytes r*2,
//         r*2+1; planes 2/3 live in the second 16 bytes at 16 + r*2, +1.
//         pixel = p0 | p1<<1 | p2<<2 | p3<<3.

#include <array>
#include <cstdint>
#include <span>

namespace snesbus {

inline constexpr int kScreenWidth = 256;
inline constexpr int kScreenHeight = 224;

// Minimal local memory model (exercise 02 has the full accessor suite).
struct Vram {
    std::array<uint16_t, 32768> w{};
};
struct Cgram {
    std::array<uint16_t, 256> e{};
};

inline uint8_t vram_byte(const Vram& v, size_t byte_addr) {
    const uint16_t word = v.w[(byte_addr >> 1) & 0x7FFFu];
    return (byte_addr & 1u) ? static_cast<uint8_t>(word >> 8)
                            : static_cast<uint8_t>(word);
}

// BGR555 -> RGBA8888 with 5->8 bit replication, alpha fully opaque.
inline uint32_t bgr555_to_rgba8(uint16_t c) {
    const auto expand = [](unsigned v) {
        return static_cast<uint32_t>((v << 3) | (v >> 2));
    };
    return 0xFF000000u | expand((c >> 10) & 0x1Fu) |
           (expand((c >> 5) & 0x1Fu) << 8) | (expand(c & 0x1Fu) << 16);
}

enum class Mode : uint8_t { Mode0, Mode1 };
enum class MathOp : uint8_t { Add, Sub };

struct LayerCfg {
    uint8_t bpp = 2;         // 2 or 4
    uint16_t tile_base = 0;  // VRAM word address of tile data
    uint16_t map_base = 0;   // VRAM word address of the 32x32 tilemap
    uint16_t hofs = 0;
    uint16_t vofs = 0;
};

struct TilemapEntry {
    uint16_t tile = 0;
    uint8_t palette = 0;  // 3 bits
    bool priority = false;
    bool hflip = false;
    bool vflip = false;
};

struct WindowRect {
    bool enable = false;
    bool invert = false;     // swap inside/outside
    uint8_t left = 0;        // inclusive
    uint8_t right = 255;     // inclusive
    uint8_t layer_mask = 0xF;  // bit n clears/keeps layer n (see window_passes)
    bool color_math_enable = false;  // math applies inside the effective window
};

struct PixelCandidate {
    uint8_t color = 0;     // tile pixel value (0 = transparent)
    uint8_t palette = 0;   // tilemap palette selector
    uint8_t layer = 0;     // 0 = BG1 .. 3 = BG4
    uint8_t priority = 0;  // tilemap priority bit
};

struct FrameCfg {
    Mode mode = Mode::Mode1;
    LayerCfg bg[4];  // Mode 1 uses bg[0..2]; Mode 0 uses bg[0..3]
    WindowRect window;
    bool color_math = false;
    MathOp math_op = MathOp::Add;
    bool math_half = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Fetch one pixel of a 2bpp tile as a 2-bit color index.
inline uint8_t tile_pixel_2bpp(const Vram& v, uint16_t base, uint16_t tile,
                               int row, int col) {
    const size_t b = static_cast<size_t>(base) * 2u +
                     static_cast<size_t>(tile) * 8u +
                     static_cast<size_t>(row) * 2u;
    const uint8_t p0 = vram_byte(v, b);
    const uint8_t p1 = vram_byte(v, b + 1);
    const uint8_t bit = static_cast<uint8_t>(7 - col);
    return static_cast<uint8_t>(((p0 >> bit) & 1u) |
                                (((p1 >> bit) & 1u) << 1));
}
//@LABS-STUB
// TODO(1): fetch a 2bpp tile pixel. A tile is 8 bytes starting at word
// address base + tile*8; row r keeps plane 0 in byte 2r and plane 1 in byte
// 2r+1; the pixel bit is bit (7-col).
inline uint8_t tile_pixel_2bpp(const Vram&, uint16_t, uint16_t, int, int) {
    return 0;  // wrong on purpose: every 2bpp pixel renders transparent
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Fetch one pixel of a 4bpp tile (32 bytes) as a 4-bit color index.
inline uint8_t tile_pixel_4bpp(const Vram& v, uint16_t base, uint16_t tile,
                               int row, int col) {
    const size_t b = static_cast<size_t>(base) * 2u +
                     static_cast<size_t>(tile) * 32u +
                     static_cast<size_t>(row) * 2u;
    const uint8_t bit = static_cast<uint8_t>(7 - col);
    const uint8_t p0 = vram_byte(v, b);
    const uint8_t p1 = vram_byte(v, b + 1);
    const uint8_t p2 = vram_byte(v, b + 16);
    const uint8_t p3 = vram_byte(v, b + 17);
    return static_cast<uint8_t>(((p0 >> bit) & 1u) |
                                (((p1 >> bit) & 1u) << 1) |
                                (((p2 >> bit) & 1u) << 2) |
                                (((p3 >> bit) & 1u) << 3));
}
//@LABS-STUB
// TODO(2): fetch a 4bpp tile pixel. Planes 0/1 sit in bytes r*2, r*2+1 of
// the tile; planes 2/3 in the second half at +16, +17.
inline uint8_t tile_pixel_4bpp(const Vram&, uint16_t, uint16_t, int, int) {
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Decode a 16-bit tilemap entry:
// bits 0-9 tile | 10-12 palette | 13 priority | 14 hflip | 15 vflip.
inline TilemapEntry decode_map_entry(uint16_t raw) {
    TilemapEntry e;
    e.tile = static_cast<uint16_t>(raw & 0x03FFu);
    e.palette = static_cast<uint8_t>((raw >> 10) & 7u);
    e.priority = (raw & 0x2000u) != 0;
    e.hflip = (raw & 0x4000u) != 0;
    e.vflip = (raw & 0x8000u) != 0;
    return e;
}
//@LABS-STUB
// TODO(3): split a 16-bit tilemap entry into its fields (bit table above).
inline TilemapEntry decode_map_entry(uint16_t) {
    return TilemapEntry{};  // wrong on purpose: every cell shows tile 0
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Sample one layer at screen position (x, y) and return its candidate.
// Color 0 means "transparent" — callers treat it as no candidate.
inline PixelCandidate sample_layer(const Vram& v, const LayerCfg& lc,
                                   uint8_t layer, int x, int y) {
    const int sx = (x + lc.hofs) & 0xFF;  // single-screen map wraps at 256 px
    const int sy = (y + lc.vofs) & 0xFF;
    const uint16_t raw =
        v.w[static_cast<size_t>(lc.map_base) +
            static_cast<size_t>(((sy >> 3) & 31u) * 32u + ((sx >> 3) & 31u))];
    const TilemapEntry e = decode_map_entry(raw);
    int tx = sx & 7;
    int ty = sy & 7;
    if (e.hflip) {
        tx = 7 - tx;
    }
    if (e.vflip) {
        ty = 7 - ty;
    }
    PixelCandidate c;
    c.color = lc.bpp == 4 ? tile_pixel_4bpp(v, lc.tile_base, e.tile, ty, tx)
                          : tile_pixel_2bpp(v, lc.tile_base, e.tile, ty, tx);
    c.palette = e.palette;
    c.layer = layer;
    c.priority = e.priority ? 1 : 0;
    return c;
}
//@LABS-STUB
// TODO(4): walk tilemap -> tilemap entry -> flipped tile pixel. Scroll
// offsets wrap modulo 256 pixels; use tile_pixel_2bpp/4bpp based on lc.bpp.
inline PixelCandidate sample_layer(const Vram&, const LayerCfg&, uint8_t layer,
                                   int, int) {
    PixelCandidate c;
    c.layer = layer;  // wrong on purpose: always transparent
    return c;
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Window gate. With the window disabled every pixel passes. Otherwise the
// rectangle [left,right] (inclusive both ends) defines "inside"; invert
// swaps inside/outside. A layer passes only when the pixel is in the
// EFFECTIVE window and its layer_mask bit is set.
inline bool window_passes(const WindowRect& w, uint8_t layer, int x) {
    if (!w.enable) {
        return true;
    }
    const bool inside = x >= w.left && x <= w.right;
    return (inside != w.invert) && ((w.layer_mask >> layer) & 1u) != 0;
}
//@LABS-STUB
// TODO(5): implement the window gate (inclusive edges, invert bit,
// per-layer enable mask).
inline bool window_passes(const WindowRect&, uint8_t, int) {
    return true;  // wrong on purpose: windows never clip anything
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
// Pick the winning candidate: minimize layer first, then maximize the
// priority bit. Encoded as the smallest value of
// key = layer * 2 + (priority ? 0 : 1). Returns -1 when every candidate is
// transparent.
inline int compose(std::span<const PixelCandidate> candidates) {
    int best = -1;
    unsigned best_key = 0xFFFFFFFFu;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const unsigned key =
            candidates[i].layer * 2u + (candidates[i].priority != 0 ? 0u : 1u);
        if (key < best_key) {
            best_key = key;
            best = static_cast<int>(i);
        }
    }
    return best;
}
//@LABS-STUB
// TODO(6): select the winner minimizing layer*2 + (priority ? 0 : 1);
// return its index, or -1 if there is nothing opaque.
inline int compose(std::span<const PixelCandidate>) {
    return -1;  // wrong on purpose: the screen stays on backdrop
}
//@LABS-END

//@LABS-BEGIN 7
//@LABS-SOLUTION
// Color math in the 5-bit-per-channel BGR555 domain. Add sums the channels
// (halving first when requested); Sub subtracts backdrop from foreground.
// Both ops saturate-clamp each channel to 0..31 AFTER the optional halving
// of the signed intermediate.
inline uint16_t apply_color_math(MathOp op, bool half, uint16_t fg,
                                 uint16_t backdrop) {
    const auto chan = [](uint16_t c, unsigned shift) {
        return static_cast<int>((c >> shift) & 0x1Fu);
    };
    const auto rebuild = [](int r, int g, int b) {
        const auto clamp5 = [](int v) {
            return static_cast<uint16_t>(v < 0 ? 0 : (v > 31 ? 31 : v));
        };
        return static_cast<uint16_t>(clamp5(r) | (clamp5(g) << 5) |
                                     (clamp5(b) << 10));
    };
    const unsigned shifts[3] = {0, 5, 10};
    int out[3];
    for (int i = 0; i < 3; ++i) {
        const int a = chan(fg, shifts[i]);
        const int b = chan(backdrop, shifts[i]);
        int d = op == MathOp::Add ? a + b : a - b;
        if (half) {
            d /= 2;  // truncation toward zero matches the hardware shifter
        }
        out[i] = d;
    }
    return rebuild(out[0], out[1], out[2]);
}
//@LABS-STUB
// TODO(7): per-channel add/sub against the backdrop in 5-bit components,
// optional halving of the intermediate, saturating clamp to 0..31.
inline uint16_t apply_color_math(MathOp, bool, uint16_t fg, uint16_t) {
    return fg;  // wrong on purpose: math never changes the color
}
//@LABS-END

namespace detail {

// Which layers a mode drives, and how a winner maps onto a CGRAM entry.
inline int layer_count(Mode m) { return m == Mode::Mode1 ? 3 : 4; }

inline uint16_t cgram_entry(Mode m, const PixelCandidate& c,
                            const Cgram& cg) {
    uint16_t idx;
    if (m == Mode::Mode1) {
        idx = c.layer == 2
                  ? static_cast<uint16_t>(32u + c.palette * 4u + c.color)
                  : static_cast<uint16_t>(c.palette * 16u + c.color);
    } else {
        idx = static_cast<uint16_t>(c.layer * 32u + c.palette * 4u + c.color);
    }
    return cg.e[idx];
}

}  // namespace detail

//@LABS-BEGIN 8
//@LABS-SOLUTION
// Render a full 256x224 frame into `out` (RGBA8888, 4 bytes per pixel).
inline void render_frame(const FrameCfg& cfg, const Vram& vram,
                         const Cgram& cgram,
                         std::span<uint8_t> out) {
    const int nlayers = detail::layer_count(cfg.mode);
    for (int y = 0; y < kScreenHeight; ++y) {
        for (int x = 0; x < kScreenWidth; ++x) {
            PixelCandidate cands[4];
            size_t n = 0;
            for (int l = 0; l < nlayers; ++l) {
                const PixelCandidate c =
                    sample_layer(vram, cfg.bg[l], static_cast<uint8_t>(l), x, y);
                if (c.color == 0 || !window_passes(cfg.window, c.layer, x)) {
                    continue;
                }
                cands[n++] = c;
            }
            uint16_t color = cgram.e[0];  // backdrop behind everything
            const int w = compose(std::span<const PixelCandidate>(cands, n));
            if (w >= 0) {
                color = detail::cgram_entry(cfg.mode, cands[w], cgram);
                const bool math_here =
                    cfg.color_math &&
                    (!cfg.window.enable || cfg.window.color_math_enable);
                if (math_here) {
                    color = apply_color_math(cfg.math_op, cfg.math_half,
                                             color, cgram.e[0]);
                }
            }
            const uint32_t rgba = bgr555_to_rgba8(color);
            const size_t o = (static_cast<size_t>(y) * kScreenWidth +
                              static_cast<size_t>(x)) * 4u;
            out[o] = static_cast<uint8_t>(rgba);
            out[o + 1] = static_cast<uint8_t>(rgba >> 8);
            out[o + 2] = static_cast<uint8_t>(rgba >> 16);
            out[o + 3] = static_cast<uint8_t>(rgba >> 24);
        }
    }
}
//@LABS-STUB
// TODO(8): for every pixel gather per-layer candidates (skip transparent /
// windowed-out ones), pick the winner with compose(), map it through the
// mode's CGRAM band (see the palette-band table above), optionally run
// color math against CGRAM entry 0, and store expanded RGBA.
inline void render_frame(const FrameCfg&, const Vram&, const Cgram&,
                         std::span<uint8_t> out) {
    // Wrong on purpose: fills flat black instead of rendering layers.
    for (size_t i = 0; i < out.size(); i += 4) {
        out[i] = out[i + 1] = out[i + 2] = 0;
        out[i + 3] = 0xFF;
    }
}
//@LABS-END

}  // namespace snesbus
