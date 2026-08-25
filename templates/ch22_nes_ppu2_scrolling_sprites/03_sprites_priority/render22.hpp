#pragma once
#include <array>
#include <cstdint>

#include "../01_loopy_scroll/loopy.hpp"
#include "../02_sprite_eval/sprite.hpp"

// Chapter 22 frame renderer: loopy-driven background scrolling plus sprite
// evaluation/priority composed into a 256x240 RGBA8 frame.
//
// Documented simplification: mid-frame register writes are not modeled
// here (the raster-timing model lives in 91_challenge). Each scanline
// simulates the hardware's per-line v updates exactly: copy_x at the
// start of visible pixels, increment_y at line end, with fine Y/coarse Y
// and nametable flips coming along automatically.
namespace nes22prio {

constexpr int kFrameW = 256;
constexpr int kFrameH = 240;

struct Rgb { uint8_t r, g, b; };

inline const std::array<Rgb, 64>& master_palette() {
    static const std::array<Rgb, 64> kPalette{{
        {84, 84, 84}, {0, 30, 116}, {8, 16, 144}, {48, 0, 136},
        {68, 0, 100}, {92, 0, 48}, {84, 4, 0}, {60, 24, 0},
        {32, 42, 0}, {8, 58, 0}, {0, 64, 0}, {0, 60, 0},
        {0, 50, 60}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
        {152, 150, 152}, {8, 76, 196}, {48, 50, 236}, {92, 30, 228},
        {136, 20, 176}, {160, 20, 100}, {152, 34, 32}, {120, 60, 0},
        {84, 90, 0}, {40, 114, 0}, {8, 124, 0}, {0, 118, 40},
        {0, 102, 120}, {0, 0, 0}, {0, 0, 0}, {236, 238, 236},
        {236, 238, 236}, {76, 154, 236}, {120, 124, 236}, {176, 98, 236},
        {228, 84, 236}, {236, 88, 180}, {236, 106, 100}, {212, 136, 32},
        {160, 170, 0}, {116, 196, 0}, {76, 208, 32}, {56, 204, 108},
        {56, 180, 204}, {60, 60, 60}, {0, 0, 0}, {236, 238, 236},
        {236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236},
        {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144},
        {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180},
        {160, 214, 228}, {160, 162, 160}, {0, 0, 0}, {236, 238, 236},
    }};
    return kPalette;
}

// Minimal PPU-bus view for nametable resolution (mirroring rules identical
// to ch21 exercise 01; duplicated so chapters stay independently buildable).
enum class Mirroring : uint8_t { Horizontal = 0, Vertical = 1 };

inline uint16_t nt_page(Mirroring m, uint16_t addr) {
    addr &= 0x0FFF;
    uint16_t page = (m == Mirroring::Vertical) ? ((addr >> 10) & 1)
                                               : ((addr >> 11) & 1);
    return uint16_t((page << 10) | (addr & 0x03FF));
}

inline int attribute_bits(uint8_t at_byte, int coarse_x, int coarse_y) {
    return (at_byte >> (((coarse_y & 2) << 1) | (coarse_x & 2))) & 3;
}

// Everything the renderer needs from a crafted scene.
struct Scene {
    Mirroring mirroring = Mirroring::Horizontal;
    const uint8_t* chr;      // 8 KB
    const uint8_t* nt;       // physical nametable RAM, 2 KB
    const uint8_t* pal;      // 32 bytes
    const uint8_t* oam;      // 256 bytes
    nes22scroll::Loopy l;
    uint8_t ctrl = 0;
    uint8_t mask = 0;
};

// Fetch the tile column `col` (hardware order: starts at copy_x(v)) for a
// scanline whose vertical state is `vline`. Returns the 2-bit tile color
// at `within` (0-7) plus the 2-bit attribute.
struct TileFetch {
    int color = 0;
    int attr = 0;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline TileFetch fetch_bg_tile(const Scene& s, nes22scroll::Loopy vline,
                               int col, int within) {
    nes22scroll::Loopy w = vline;
    for (int i = 0; i < col; ++i) nes22scroll::increment_x(w);
    uint16_t nt_addr = uint16_t(0x2000 | (w.v & 0x0FFF));
    uint8_t tile = s.nt[nt_page(s.mirroring, nt_addr)];
    // Attribute byte: $23C0-equivalent region, quadrant by coarse coords.
    uint16_t at_addr =
        uint16_t(0x2000 | (w.v & 0x0C00) | 0x03C0 |
                 ((w.v >> 4) & 0x38) | ((w.v >> 2) & 0x07));
    uint8_t at = s.nt[nt_page(s.mirroring, at_addr)];
    int attr = attribute_bits(at, w.v & 0x1F, (w.v >> 5) & 0x1F);

    int plane_base = (s.ctrl & 0x10) ? 0x1000 : 0x0000;
    int fine_y = (w.v >> 12) & 0x07;
    int off = plane_base + tile * 16 + fine_y;
    uint8_t low = s.chr[off];
    uint8_t high = s.chr[off + 8];
    int bit = 7 - within;
    TileFetch tf;
    tf.color = ((low >> bit) & 1) | (((high >> bit) & 1) << 1);
    tf.attr = attr;
    return tf;
}
//@LABS-STUB
// TODO(1): walk `col` increment_x steps from a copy of vline, then fetch
// nametable byte, attribute byte (address 0x2000|(v&0x0C00)|0x3C0|
// ((v>>4)&0x38)|((v>>2)&0x07)) and both pattern planes with fine Y from
// bits 12-14 and the PPUCTRL bit-4 plane base. Stub returns transparent.
inline TileFetch fetch_bg_tile(const Scene& /*s*/, nes22scroll::Loopy /*vline*/,
                               int /*col*/, int /*within*/) {
    return {};
}
//@LABS-END

// Background color for one pixel (0 = transparent / backdrop).
//
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline int bg_pixel(const Scene& s, nes22scroll::Loopy vline, int px,
                    int fine_x) {
    if ((s.mask & 0x08) == 0) return 0;              // bg rendering off
    if (px < 8 && (s.mask & 0x02) == 0) return 0;    // left-column clip
    int col = (fine_x + px) >> 3;
    int within = (fine_x + px) & 7;
    TileFetch tf = fetch_bg_tile(s, vline, col, within);
    if (tf.color == 0) return 0;                     // backdrop
    return (tf.attr << 2) | tf.color;                // $3F00-$3F0F index
}
//@LABS-STUB
// TODO(2): honor PPUMASK bit 3 (bg enable) and bit 1 (left-column clip),
// then pick tile column (fine_x + px) >> 3 and decode via fetch_bg_tile;
// color 0 -> 0, else (attr << 2) | color. Stub returns 0 always.
inline int bg_pixel(const Scene& /*s*/, nes22scroll::Loopy /*vline*/, int /*px*/,
                    int /*fine_x*/) {
    return 0;
}
//@LABS-END

// Sprite pixel: returns palette-RAM index (0x10|...) for the winning
// sprite at px, or 0 when transparent. `slots` comes from evaluation.
struct SpritePixel {
    int ram_index = 0;      // full $3F10-$3F1F index, 0 if none
    bool behind = false;    // priority bit of winning sprite
    bool is_sprite0 = false;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline SpritePixel sprite_pixel(const Scene& s,
                                const nes22sprite::EvalResult& ev, int line,
                                int px) {
    SpritePixel out;
    if ((s.mask & 0x10) == 0) return out;            // sprite rendering off
    bool left_ok = px >= 8 || (s.mask & 0x04) != 0;  // left-column clip
    if (!left_ok) return out;

    bool tall = (s.ctrl & 0x20) != 0;
    for (int i = 0; i < ev.count; ++i) {
        const nes22sprite::OamEntry e = nes22sprite::oam_get(s.oam, ev.slots[i]);
        int dx = px - e.x;
        if (dx < 0 || dx > 7) continue;
        int row = line - e.y;
        int h = tall ? 16 : 8;
        if (row < 0 || row >= h) continue;
        if (e.attr & 0x80) row = (h - 1) - row;      // vertical flip
        int col = (e.attr & 0x40) ? 7 - dx : dx;     // horizontal flip

        uint8_t tile = e.tile;
        int bank, plane_off;
        if (tall) {
            bank = tile & 0x01;
            tile = uint8_t((tile & 0xFE) + ((row >= 8) ? 1 : 0));
            plane_off = bank * 0x1000 + tile * 16 + (row & 7);
        } else {
            bank = (s.ctrl & 0x08) ? 1 : 0;
            plane_off = bank * 0x1000 + tile * 16 + row;
        }
        uint8_t low = s.chr[plane_off];
        uint8_t high = s.chr[plane_off + 8];
        int bit = 7 - col;
        int color = ((low >> bit) & 1) | (((high >> bit) & 1) << 1);
        if (color == 0) continue;                    // try next sprite
        out.ram_index = 0x10 | ((e.attr & 0x03) << 2) | color;
        out.behind = (e.attr & 0x20) != 0;
        out.is_sprite0 = (ev.slots[i] == 0);
        return out;                                  // first opaque wins
    }
    return out;
}
//@LABS-STUB
// TODO(3): scan evaluated slots in order; skip sprites not covering px or
// row; apply vertical/horizontal flips (attr bits 7/6); fetch pattern
// (8x16 mode: odd/even tile pair, bank from tile bit 0; 8x8: PPUCTRL
// bit 3); first OPAQUE pixel wins: set ram_index 0x10|(pal<<2)|color,
// behind flag from attr bit 5, is_sprite0 for OAM slot 0. Stub: nothing.
inline SpritePixel sprite_pixel(const Scene& /*s*/,
                                const nes22sprite::EvalResult& /*ev*/,
                                int /*line*/, int /*px*/) {
    return {};
}
//@LABS-END

// Vertical state of scanline `line` given the fixture's t register: the
// pre-render line applies copy_y once, then increment_y after each line.
inline nes22scroll::Loopy vline_for(const Scene& s, int line) {
    nes22scroll::Loopy w{s.l.v, s.l.t, s.l.x, s.l.w};
    nes22scroll::copy_y(w);              // pre-render reload
    for (int i = 0; i < line; ++i) nes22scroll::increment_y(w);
    return w;
}

// Compose a full frame. Returns true if a sprite-0 hit was detected.
template <size_t N>
bool render_frame(std::array<uint8_t, N>& out, const Scene& s) {
    static_assert(N == size_t(kFrameW) * kFrameH * 4, "bad framebuffer size");
    const auto& mpal = master_palette();
    bool hit = false;
    for (int line = 0; line < kFrameH; ++line) {
        nes22scroll::Loopy vl = vline_for(s, line);
        bool ovf_dummy = false;
        nes22sprite::EvalResult ev =
            nes22sprite::evaluate(s.oam, line, (s.ctrl & 0x20) != 0, ovf_dummy);
        for (int px = 0; px < kFrameW; ++px) {
            int bg = bg_pixel(s, vl, px, s.l.x);
            SpritePixel sp = sprite_pixel(s, ev, line, px);
            bool show_sprite = sp.ram_index != 0 &&
                               (!sp.behind || bg == 0);
            int ram_idx = show_sprite ? sp.ram_index : bg;   // 0 = backdrop
            if (sp.is_sprite0 && sp.ram_index != 0 && bg != 0) {
                nes22sprite::MaskBits m{(s.mask & 0x02) == 0,
                                        (s.mask & 0x04) == 0, true, true};
                if (nes22sprite::sprite0_hit_at(px, true, true, true, m))
                    hit = true;
            }
            const Rgb& c = mpal[s.pal[ram_idx & 0x1F]];
            size_t o = (size_t(line) * kFrameW + px) * 4;
            out[o] = c.r; out[o + 1] = c.g; out[o + 2] = c.b;
            out[o + 3] = 0xFF;
        }
    }
    return hit;
}

}  // namespace nes22prio
