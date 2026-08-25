#define LABSTEST_MAIN
#include "labstest.hpp"

#include "render.hpp"

using namespace snesbus;

namespace {

// Write one byte into the word-addressed VRAM image.
void vb(Vram& v, size_t byte_addr, uint8_t value) {
    uint16_t& w = v.w[(byte_addr >> 1) & 0x7FFFu];
    if (byte_addr & 1u) {
        w = static_cast<uint16_t>((w & 0x00FFu) | (value << 8));
    } else {
        w = static_cast<uint16_t>((w & 0xFF00u) | value);
    }
}

// Fill a 2bpp tile: every pixel becomes ((r + c) % 4).
void fill_tile_2bpp(Vram& v, uint16_t base, uint16_t tile) {
    const size_t b = static_cast<size_t>(base) * 2u + tile * 8u;
    for (int r = 0; r < 8; ++r) {
        uint8_t p0 = 0;
        uint8_t p1 = 0;
        for (int c = 0; c < 8; ++c) {
            const unsigned px = static_cast<unsigned>(r + c) % 4u;
            p0 |= static_cast<uint8_t>((px & 1u) << (7 - c));
            p1 |= static_cast<uint8_t>(((px >> 1) & 1u) << (7 - c));
        }
        vb(v, b + r * 2u, p0);
        vb(v, b + r * 2u + 1, p1);
    }
}

// Fill a 4bpp tile so every pixel shows `color`.
void fill_tile_4bpp(Vram& v, size_t byte_base, uint8_t color) {
    for (int r = 0; r < 8; ++r) {
        uint8_t planes[4] = {0, 0, 0, 0};
        for (int c = 0; c < 8; ++c) {
            for (unsigned p = 0; p < 4; ++p) {
                planes[p] |=
                    static_cast<uint8_t>(((color >> p) & 1u) << (7 - c));
            }
        }
        vb(v, byte_base + r * 2u, planes[0]);
        vb(v, byte_base + r * 2u + 1, planes[1]);
        vb(v, byte_base + 16u + r * 2u, planes[2]);
        vb(v, byte_base + 17u + r * 2u, planes[3]);
    }
}

uint32_t pixel_rgba(
    const std::array<uint8_t, kScreenWidth * kScreenHeight * 4>& f, int x,
    int y) {
    const size_t o = (static_cast<size_t>(y) * kScreenWidth + x) * 4u;
    return f[o] | (f[o + 1] << 8) | (f[o + 2] << 16) | (f[o + 3] << 24);
}

// Mode 1 baseline scene used by the frame-level tests:
// BG1 opaque green everywhere (palette 2 -> CGRAM 37),
// BG2 opaque red everywhere (palette 0 -> CGRAM 9),
// BG3 fully transparent.
struct Baseline {
    Vram vram;
    Cgram cgram;
    FrameCfg cfg;
};

Baseline make_baseline() {
    Baseline bl;
    FrameCfg& cfg = bl.cfg;
    cfg.mode = Mode::Mode1;

    // BG1: 4bpp tiles at word 0, tile 1 = color 5; map at word 1024.
    cfg.bg[0] = LayerCfg{4, 0, 1024, 0, 0};
    fill_tile_4bpp(bl.vram, 1u * 32u, 5);
    for (unsigned i = 0; i < 1024; ++i) {
        bl.vram.w[1024 + i] = 0x2801;  // prio | palette 2 | tile 1
    }

    // BG2: 4bpp tiles at word 2048, tile 1 = color 9; map at word 3072.
    cfg.bg[1] = LayerCfg{4, 2048, 3072, 0, 0};
    fill_tile_4bpp(bl.vram, 2048u * 2u + 32u, 9);
    for (unsigned i = 0; i < 1024; ++i) {
        bl.vram.w[3072 + i] = 0x8001;  // vflip | palette 0 | prio 0 | tile 1
    }

    // BG3: 2bpp, left fully empty (transparent).
    cfg.bg[2] = LayerCfg{2, 4096, 5120, 0, 0};

    // Backdrop dark blue; winners green/red in their bands.
    bl.cgram.e[0] = 0x3800u;
    bl.cgram.e[2 * 16 + 5] = 0x03E0u;  // full green
    bl.cgram.e[9] = 0x001Fu;           // full red

    cfg.window.enable = false;
    cfg.color_math = false;
    return bl;
}

Baseline make_windowed() {
    Baseline bl = make_baseline();
    bl.cfg.window.enable = true;
    bl.cfg.window.left = 100;
    bl.cfg.window.right = 155;
    bl.cfg.window.layer_mask = 0b0001;  // inside the window only BG1 passes
    return bl;
}

constexpr size_t kFbBytes = kScreenWidth * kScreenHeight * 4;

}  // namespace

TEST(tile2bpp, plane_layout) {
    Vram v;
    const size_t b = 10u * 2u + 3u * 8u;  // base word 10, tile 3, byte view
    vb(v, b, 0xF0);
    vb(v, b + 1, 0x0F);
    EXPECT_EQ(tile_pixel_2bpp(v, 10, 3, 0, 0), 1u);  // plane 0 only
    EXPECT_EQ(tile_pixel_2bpp(v, 10, 3, 0, 7), 2u);  // plane 1 only
    EXPECT_EQ(tile_pixel_2bpp(v, 10, 3, 1, 0), 0u);  // untouched row

    fill_tile_2bpp(v, 20, 5);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            EXPECT_EQ(tile_pixel_2bpp(v, 20, 5, r, c),
                      static_cast<unsigned>(r + c) % 4u);
        }
    }
}

TEST(tile4bpp, plane_layout) {
    Vram v;
    const size_t b = 7u * 2u + 2u * 32u;  // base word 7, tile 2
    vb(v, b, 0b10100000);                 // row 0 plane 0: cols 0,2
    vb(v, b + 1, 0b01010000);             // row 0 plane 1: cols 1,3
    vb(v, b + 16, 0b00001000);            // row 0 plane 2: col 4
    vb(v, b + 17, 0b00000100);            // row 0 plane 3: col 5
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 0), 1u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 1), 2u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 2), 1u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 3), 2u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 4), 4u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 5), 8u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 0, 6), 0u);
    EXPECT_EQ(tile_pixel_4bpp(v, 7, 2, 1, 0), 0u);  // next row untouched
}

TEST(mapentry, decode_vectors) {
    const TilemapEntry e = decode_map_entry(0xB42Fu);
    EXPECT_EQ(e.tile, 0x02Fu);   // bits 0-9
    EXPECT_EQ(e.palette, 5u);    // bits 10-12 = 0b101
    EXPECT_TRUE(e.priority);     // bit 13
    EXPECT_FALSE(e.hflip);       // bit 14 clear
    EXPECT_TRUE(e.vflip);        // bit 15 set
    const TilemapEntry flat = decode_map_entry(0x8001);
    EXPECT_FALSE(flat.priority);
    EXPECT_FALSE(flat.hflip);
    EXPECT_TRUE(flat.vflip);
    EXPECT_EQ(flat.tile, 1u);
}

TEST(sample, scroll_wrap_and_flips) {
    Vram v;
    FrameCfg cfg;
    cfg.mode = Mode::Mode1;
    cfg.bg[0] = LayerCfg{2, 0, 100, 0, 0};  // 2bpp tiles @0, map @100
    fill_tile_2bpp(v, 0, 1);

    // Map cell (0,0) -> tile 1 with priority+hflip+vflip+palette 2.
    v.w[100] = 0xE801;
    const PixelCandidate c = sample_layer(v, cfg.bg[0], 0, 0, 0);
    EXPECT_EQ(c.layer, 0u);
    EXPECT_EQ(c.priority, 1u);
    EXPECT_EQ(c.palette, 2u);
    // Both flips map screen (0,0) to pattern pixel (7,7): (7+7)%4 = 2.
    EXPECT_EQ(c.color, 2u);

    // Scroll (256,256) wraps back onto the same map cell.
    cfg.bg[0].hofs = 256;
    cfg.bg[0].vofs = 256;
    EXPECT_EQ(sample_layer(v, cfg.bg[0], 0, 0, 0).color, 2u);

    // Scroll by 1: tile column 1 flips to pattern column 6 -> (7+6)%4 = 1.
    cfg.bg[0].hofs = 1;
    cfg.bg[0].vofs = 0;
    EXPECT_EQ(sample_layer(v, cfg.bg[0], 0, 0, 0).color, 1u);
}

TEST(window, edges_invert_and_mask) {
    WindowRect w;
    w.enable = true;
    w.left = 10;
    w.right = 19;
    EXPECT_TRUE(window_passes(w, 0, 10));  // left edge inclusive
    EXPECT_TRUE(window_passes(w, 1, 19));  // right edge inclusive
    EXPECT_FALSE(window_passes(w, 0, 9));
    EXPECT_FALSE(window_passes(w, 0, 20));

    w.invert = true;  // now OUTSIDE is the effective window
    EXPECT_FALSE(window_passes(w, 0, 15));
    EXPECT_TRUE(window_passes(w, 0, 25));

    w.invert = false;
    w.layer_mask = 0b0010;  // only layer 1 may pass
    EXPECT_FALSE(window_passes(w, 0, 12));
    EXPECT_TRUE(window_passes(w, 1, 12));

    w.enable = false;
    EXPECT_TRUE(window_passes(w, 3, 0));  // disabled window never clips
}

TEST(compose, layer_precedence_and_priority) {
    constexpr PixelCandidate bg0p0{5, 0, 0, 0};
    constexpr PixelCandidate bg0p1{6, 0, 0, 1};
    constexpr PixelCandidate bg1p1{7, 0, 1, 1};
    constexpr PixelCandidate bg2p1{8, 0, 2, 1};

    const PixelCandidate all[] = {bg2p1, bg1p1, bg0p0};
    EXPECT_EQ(compose(std::span<const PixelCandidate>(all)), 2);

    const PixelCandidate tie[] = {bg1p1, bg0p1};
    EXPECT_EQ(compose(std::span<const PixelCandidate>(tie)), 1);

    const PixelCandidate deep[] = {bg2p1, bg1p1};
    EXPECT_EQ(compose(std::span<const PixelCandidate>(deep)), 1);

    EXPECT_EQ(compose(std::span<const PixelCandidate>()), -1);
}

TEST(colormath, add_sub_half_clamp_vectors) {
    const uint16_t white = 0x7FFF;  // (31,31,31)
    const uint16_t red = 0x001F;    // (31,0,0)
    // Add saturates at 31 per channel.
    EXPECT_EQ(apply_color_math(MathOp::Add, false, white, red), 0x7FFF);
    // Add-half of the SUM: white+red -> (31,31,31)+(31,0,0) halved =
    // (31,15,15) in (r,g,b).
    EXPECT_EQ(apply_color_math(MathOp::Add, true, white, red),
              (15u << 10) | (15u << 5) | 31u);
    // Sub below zero clamps to 0.
    EXPECT_EQ(apply_color_math(MathOp::Sub, false, red, white), 0u);
    // Sub: white - red = (0,31,31).
    EXPECT_EQ(apply_color_math(MathOp::Sub, false, white, red),
              (31u << 10) | (31u << 5));
    // Sub-half truncates toward zero: (31-30)/2 = 0.
    const uint16_t almost_white = 0x7BDE;  // (30,30,30)
    EXPECT_EQ(apply_color_math(MathOp::Sub, true, white, almost_white), 0u);
}

TEST(frame, mode1_layer_order_baseline) {
    const Baseline bl = make_baseline();
    std::array<uint8_t, kFbBytes> fb{};
    render_frame(bl.cfg, bl.vram, bl.cgram, fb);
    // BG1 wins everywhere despite lower-priority BG2 underneath.
    EXPECT_EQ(pixel_rgba(fb, 0, 0), bgr555_to_rgba8(0x03E0));
    EXPECT_EQ(pixel_rgba(fb, 255, 223), bgr555_to_rgba8(0x03E0));
}

TEST(frame, window_shows_masked_layers_inside_rect) {
    std::array<uint8_t, kFbBytes> fb{};
    render_frame(make_windowed().cfg, make_windowed().vram,
                 make_windowed().cgram, fb);
    // Window [100,155] with mask 0b0001: BG1 shows ONLY inside the rect;
    // outside nothing passes -> backdrop.
    EXPECT_EQ(pixel_rgba(fb, 50, 10), bgr555_to_rgba8(0x3800));
    EXPECT_EQ(pixel_rgba(fb, 120, 10), bgr555_to_rgba8(0x03E0));

    Baseline alt = make_windowed();
    alt.cfg.window.layer_mask = 0b1110;  // inside: BG2 survives, not BG1
    std::array<uint8_t, kFbBytes> fb2{};
    render_frame(alt.cfg, alt.vram, alt.cgram, fb2);
    EXPECT_EQ(pixel_rgba(fb2, 120, 10), bgr555_to_rgba8(0x001F));  // BG2 red
    EXPECT_EQ(pixel_rgba(fb2, 156, 10), bgr555_to_rgba8(0x3800));  // backdrop
}

TEST(frame, window_invert_moves_visible_region_outside) {
    Baseline inv = make_windowed();
    inv.cfg.window.invert = true;  // effective window becomes OUTSIDE rect
    std::array<uint8_t, kFbBytes> fb{};
    render_frame(inv.cfg, inv.vram, inv.cgram, fb);
    EXPECT_EQ(pixel_rgba(fb, 130, 5), bgr555_to_rgba8(0x3800));  // inside
    EXPECT_EQ(pixel_rgba(fb, 5, 5), bgr555_to_rgba8(0x03E0));    // outside
}

TEST(frame, color_math_and_window_gate) {
    // Probe INSIDE the window where BG1 (mask 0b0001) survives.
    const int px = 120;
    // Add-half: winner green (g=31) vs backdrop 0x3800 (b=14) gives
    // (r,g,b) = (0,15,7) -> BGR555 (7<<10)|(15<<5).
    Baseline bl = make_windowed();
    bl.cfg.color_math = true;
    bl.cfg.window.color_math_enable = true;
    bl.cfg.math_half = true;
    std::array<uint8_t, kFbBytes> fb{};
    render_frame(bl.cfg, bl.vram, bl.cgram, fb);
    EXPECT_EQ(pixel_rgba(fb, px, 60),
              bgr555_to_rgba8(static_cast<uint16_t>((7u << 10) | (15u << 5))));

    // Window active but color-math enable clear: math never applies.
    Baseline gated = make_windowed();
    gated.cfg.color_math = true;
    gated.cfg.window.color_math_enable = false;
    std::array<uint8_t, kFbBytes> fb2{};
    render_frame(gated.cfg, gated.vram, gated.cgram, fb2);
    EXPECT_EQ(pixel_rgba(fb2, px, 60), bgr555_to_rgba8(0x03E0));

    // Window active and color-math enable set: plain add applies —
    // green + backdrop = (r,g,b) = (0,31,14) -> BGR555 (14<<10)|(31<<5).
    Baseline armed = gated;
    armed.cfg.window.color_math_enable = true;
    std::array<uint8_t, kFbBytes> fb3{};
    render_frame(armed.cfg, armed.vram, armed.cgram, fb3);
    EXPECT_EQ(pixel_rgba(fb3, px, 60),
              bgr555_to_rgba8(static_cast<uint16_t>((14u << 10) | (31u << 5))));
}

TEST(frame, mode0_palette_bands) {
    Vram vram;
    Cgram cgram;
    FrameCfg cfg;
    cfg.mode = Mode::Mode0;
    for (int l = 0; l < 4; ++l) {
        cfg.bg[l] = LayerCfg{2, 0,
                             static_cast<uint16_t>(2048u +
                                                   static_cast<unsigned>(l)),
                             0, 0};
    }
    // One shared opaque tile (color 3) at word 0.
    for (int r = 0; r < 8; ++r) {
        vb(vram, r * 2u, 0xFF);      // plane 0
        vb(vram, r * 2u + 1, 0xFF);  // plane 1
    }
    for (int l = 0; l < 4; ++l) {
        vram.w[2048u + static_cast<unsigned>(l)] =
            static_cast<uint16_t>(0x2000u | (1u << 10));  // prio, palette 1
    }
    // Band mapping: layer n, palette 1, color 3 -> entry n*32 + 4 + 3.
    cgram.e[0 * 32 + 4 + 3] = 0x001Fu;
    cgram.e[1 * 32 + 4 + 3] = 0x03E0u;
    cgram.e[2 * 32 + 4 + 3] = 0x7C00u;
    cgram.e[3 * 32 + 4 + 3] = 0x7FFFu;

    std::array<uint8_t, kFbBytes> fb{};
    render_frame(cfg, vram, cgram, fb);
    EXPECT_EQ(pixel_rgba(fb, 0, 0), bgr555_to_rgba8(0x001F));  // BG1 wins
}
