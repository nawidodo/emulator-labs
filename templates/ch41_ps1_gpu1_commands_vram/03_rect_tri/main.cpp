#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "raster.hpp"

using psx::gpu::DrawConfig;
using psx::gpu::RasterVert;
using psx::gpu::Vram;

namespace {
RasterVert vert(int x, int y, uint32_t rgb = 0) {
    RasterVert v;
    v.x = x;
    v.y = y;
    v.b = (rgb >> 16) & 0xFF;
    v.g = (rgb >> 8) & 0xFF;
    v.r = rgb & 0xFF;
    return v;
}
}  // namespace

TEST(rect, zero_size_degenerates_to_maximum) {
    Vram v;
    DrawConfig cfg;
    psx::gpu::draw_rectangle(v, cfg, 0xFFFFFF, 10, 20, 0, 0);
    EXPECT_EQ(v.at(10, 20), 0x7FFFu);
    EXPECT_EQ(v.at(1023, 511), 0x7FFFu);
}

TEST(rect, drawing_offset_and_area_clip) {
    Vram v;
    DrawConfig cfg;
    cfg.area_x1 = 100;
    cfg.area_y1 = 50;
    cfg.area_x2 = 199;
    cfg.area_y2 = 99;
    cfg.off_x = 95;
    cfg.off_y = 45;
    // Rect occupies (95..158, 45..76) after offset; clipped to the area.
    psx::gpu::draw_rectangle(v, cfg, 0xF80000, 0, 0, 64, 32);
    EXPECT_EQ(v.at(99, 50), 0u);           // clipped left
    EXPECT_EQ(v.at(100, 49), 0u);          // clipped top
    EXPECT_EQ(v.at(100, 50), (31u << 10)); // pure blue: 0xF8 -> 5bit 31
    EXPECT_EQ(v.at(158, 76), (31u << 10));
    EXPECT_EQ(v.at(159, 76), 0u);          // beyond width
    EXPECT_EQ(v.at(158, 77), 0u);          // beyond height
}

TEST(rect, check_mask_protects_set_mask_forces_bit15) {
    Vram v;
    v.at(5, 5) = 0x8001;  // pre-marked pixel
    DrawConfig cfg;
    cfg.check_mask = true;
    psx::gpu::draw_rectangle(v, cfg, 0x00FF00, 0, 0, 16, 16);
    EXPECT_EQ(v.at(5, 5), 0x8001u);  // write-protected
    EXPECT_EQ(v.at(6, 6), 0x03E0u);  // normal pixel
    DrawConfig cfg2;
    cfg2.set_mask = true;
    psx::gpu::draw_rectangle(v, cfg2, 0x00FF00, 20, 20, 2, 1);
    EXPECT_EQ(v.at(20, 20), 0x83E0u);  // bit15 forced
}

TEST(raster, signed_area_sign_drives_culling) {
    Vram v;
    DrawConfig cfg;
    const RasterVert a = vert(2, 2, 0xFF);
    const RasterVert b = vert(6, 2, 0xFF);
    const RasterVert c = vert(2, 6, 0xFF);
    EXPECT_TRUE(psx::gpu::signed_area2(a, b, c) > 0);  // clockwise: front
    psx::gpu::draw_triangle_flat(v, cfg, 0xFF, a, b, c);
    EXPECT_EQ(v.at(3, 2), 0x001Fu);
    // Reverse the winding: same geometry, culled.
    Vram v2;
    psx::gpu::draw_triangle_flat(v2, cfg, 0xFF, a, c, b);
    EXPECT_EQ(v2.at(3, 2), 0u);
}

TEST(raster, flat_triangle_exact_pixel_set_top_left_rule) {
    // Right triangle (2,2)-(6,2)-(2,6): rows hold 4/3/2/1 pixels; the
    // hypotenuse centres belong to the primitive (dy>0 edge), while the
    // right column and bottom row are excluded.
    Vram v;
    DrawConfig cfg;
    psx::gpu::draw_triangle_flat(v, cfg, 0xFF, vert(2, 2), vert(6, 2),
                                 vert(2, 6));
    int count = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 1024; ++x) {
            if (v.at(x, y) == 0) continue;
            ++count;
            EXPECT_EQ(v.at(x, y), 0x001Fu);
        }
    EXPECT_EQ(count, 10);
    EXPECT_TRUE(v.at(5, 2) != 0);   // hypotenuse centre included
    EXPECT_TRUE(v.at(2, 5) != 0);
    EXPECT_TRUE(v.at(6, 2) == 0);   // right edge excluded
    EXPECT_TRUE(v.at(2, 6) == 0);   // bottom edge excluded
}

TEST(raster, shared_edge_drawn_exactly_once) {
    // Two front-facing triangles tile the square (0,0)-(4,4). The shared
    // diagonal is traversed DOWNWARD-LEFT by tri1 (not top/left: excluded)
    // and UPWARD-RIGHT by tri2 (top/left: included) -> exactly one owner.
    Vram t1, t2, both;
    DrawConfig cfg;
    psx::gpu::draw_triangle_flat(t1, cfg, 0xFF, vert(0, 0), vert(4, 0),
                                 vert(4, 4));
    psx::gpu::draw_triangle_flat(t2, cfg, 0xFF, vert(0, 0), vert(4, 4),
                                 vert(0, 4));
    psx::gpu::draw_triangle_flat(both, cfg, 0xFF, vert(0, 0), vert(4, 0),
                                 vert(4, 4));
    psx::gpu::draw_triangle_flat(both, cfg, 0xFF, vert(0, 0), vert(4, 4),
                                 vert(0, 4));
    int overlap = 0;
    int filled = 0;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            if (t1.at(x, y) != 0 && t2.at(x, y) != 0) ++overlap;
            if (both.at(x, y) != 0) ++filled;
        }
    EXPECT_EQ(overlap, 0);  // no double-drawn shared edge
    EXPECT_EQ(filled, 16);  // and no gaps either
}

TEST(raster, gouraud_gradient_matches_q12_spec) {
    // A(0,0)/black, B(10,0)/black, C(0,10)/blue250. Pixel (1,1):
    // lambda_c = 60<<12/400 = 614 -> channel (614*250+2048)>>12 = 37 ->
    // 5bit 37>>3 = 4. Deterministic golden for the coding-test contract.
    Vram v;
    DrawConfig cfg;
    psx::gpu::draw_triangle_gouraud(v, cfg, vert(0, 0), vert(10, 0),
                                    vert(0, 10, 0xFA0000));
    EXPECT_EQ(v.at(1, 1), 4u << 10);
    // Pixel (1,0): lambda_c = 20<<12/400 = 204 -> (204*250+2048)>>12 = 12
    // -> 5bit 1.
    EXPECT_EQ(v.at(1, 0), 1u << 10);
    // Corner pixel centres lie ON the hypotenuse boundary... but the
    // hypotenuse is top/left here, so row/col 0 pixels up to the corner
    // ARE drawn; the vertex (0,10) itself is not (its centre is outside).
    EXPECT_TRUE(v.at(0, 9) != 0);
    EXPECT_EQ(v.at(0, 10), 0u);
}

TEST(packet, mono_quad_rasterizes_both_halves) {
    // GP0(28h) packet ordered around the perimeter (TL,TR,BR,BL): hardware
    // splits into (1,2,3)+(2,3,4). The upper-left sliver between the two
    // halves stays UNDRAWN — a documented consequence of the literal
    // PSX-SPX split combined with our fill convention.
    Vram v;
    DrawConfig cfg;
    const uint32_t prm[5] = {
        0x280000FFu,                    // opaque mono quad, red
        (0 << 11) | 0,                  // v1 TL (0,0)
        (0 << 11) | 6,                  // v2 TR (6,0)
        (4 << 11) | 6,                  // v3 BR (6,4)
        (4 << 11) | 0,                  // v4 BL (0,4)
    };
    psx::gpu::draw_render_packet(v, cfg, 0x28, prm);
    EXPECT_EQ(v.at(0, 0), 0u);       // TL corner centre falls in the sliver
    EXPECT_EQ(v.at(3, 0), 0x001Fu);  // tri1 half
    EXPECT_EQ(v.at(1, 0), 0x001Fu);
    EXPECT_EQ(v.at(5, 3), 0x001Fu);  // tri2 half
    EXPECT_EQ(v.at(6, 4), 0u);       // BR corner: right/bottom excluded
    EXPECT_EQ(v.at(0, 3), 0u);       // the documented sliver
    EXPECT_EQ(v.at(7, 0), 0u);
    EXPECT_EQ(v.at(6, 5), 0u);
}

TEST(packet, shaded_tri_packet_wires_colors_to_vertices) {
    Vram v;
    DrawConfig cfg;
    const uint32_t prm[6] = {
        0x300000FFu,                    // cmd 30h, color1 = pure red
        (0 << 11) | 0,                  // v1 (0,0)  red
        0x0000FF00u,                    // color2 = pure green
        (0 << 11) | 8,                  // v2 (8,0)  green
        0x00FF0000u,                    // color3 = pure blue
        (8 << 11) | 0,                  // v3 (0,8)  blue
    };
    psx::gpu::draw_render_packet(v, cfg, 0x30, prm);
    // Pixel (1,1): lambda = (2560,768,768)*255 -> channels 159/48/48 ->
    // 5-bit 19/6/6.
    EXPECT_EQ(v.at(1, 1), (6u << 10) | (6u << 5) | 19u);
    // Pixel (1,0): lambda = (3072,768,256)*255 -> channels 191/48/16 ->
    // 5-bit 23/6/2.
    EXPECT_EQ(v.at(1, 0), (2u << 10) | (6u << 5) | 23u);
}
