#pragma once
// Exercise 03 (chapter starter) — untextured primitive rasterization.
//
// Scope: GP0 monochrome/shaded triangles and quads (20h/22h/28h/2Ah,
// 30h/32h/38h/3Ah) and variable-size rectangles (60h..62h). Semi-transparent
// opcode variants render opaque here; real blending arrives in ch42, as do
// texturing and dithering (this chapter's baseline runs with dither OFF).
//
// Documented deterministic rules (see SPEC.md for the rationale):
//   * Sample rule: a pixel is owned by its CENTER (px+0.5, py+0.5),
//     evaluated on an integer lattice at (2*px+1, 2*py+1).
//   * Top-left fill convention: boundary centres are included for TOP edges
//     (horizontal, pointing +x) and LEFT edges (pointing +y in y-down
//     screen space); right/bottom boundaries are excluded, so primitives
//     never overdraw a shared edge.
//   * Backface culling: signed area <= 0 draws nothing. Positive signed
//     area (clockwise on a y-down screen) faces the viewer.
//   * Gouraud channels interpolate in Q12 fixed point with round-half-up.
#include "../shared/vram.hpp"
#include <cstdint>
#include <span>

namespace psx::gpu {

inline constexpr int imin(int a, int b) { return a < b ? a : b; }
inline constexpr int imax(int a, int b) { return a > b ? a : b; }

// Sign-extend an 11-bit vertex coordinate field.
inline constexpr int sext11(uint32_t v) {
    v &= 0x7FFu;
    return (v & 0x400u) != 0u ? static_cast<int>(v) - 0x800
                              : static_cast<int>(v);
}

struct DrawConfig {
    int area_x1 = 0, area_y1 = 0;                  // drawing area (inclusive)
    int area_x2 = kVramWidth - 1, area_y2 = kVramHeight - 1;
    int off_x = 0, off_y = 0;                      // drawing offset (E5h)
    bool set_mask = false;                         // E6h bit0
    bool check_mask = false;                       // E6h bit1
};

struct RasterVert {
    int x = 0, y = 0;              // screen coords, drawing offset applied
    uint32_t r = 0, g = 0, b = 0;  // 8-bit command color
};

// Final pixel write: clip against the drawing area, honour the check-mask
// (destination bit15 write-protects the pixel) and force bit15 when the
// set-mask bit is on. Untextured pixels otherwise store bit15=0.
inline void write_pixel(Vram& v, const DrawConfig& cfg, int x, int y,
                        uint32_t rgb888) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (x < cfg.area_x1 || x > cfg.area_x2 || y < cfg.area_y1 ||
        y > cfg.area_y2)
        return;
//@LABS-STUB
    // BUG(1): upper bounds treated as EXCLUSIVE. The PSX drawing area is
    // INCLUSIVE on both ends, so every primitive loses its final row and
    // column - one-pixel borders vanish and golden frames never match.
    if (x < cfg.area_x1 || x >= cfg.area_x2 || y < cfg.area_y1 ||
        y >= cfg.area_y2)
        return;   // wrong on purpose
//@LABS-END

    uint16_t& dst = v.at(x, y);
    if (cfg.check_mask && (dst & 0x8000u) != 0u) return;
    dst = rgb888_to_bgr555(rgb888) | (cfg.set_mask ? 0x8000u : 0x0000u);
}

// GP0(60h)-style variable-size monochrome rectangle. Size fields follow the
// COPY normalisation: 0 degenerates to the maximum (1024 x 512), anything
// else clips to its field. The drawing offset applies; clipping happens
// against the drawing area (transfers/fill never clip like this).
inline void draw_rectangle(Vram& v, const DrawConfig& cfg, uint32_t rgb888,
                           int x, int y, uint32_t w, uint32_t h) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    w = copy_width(w);
    h = copy_height(h);
    x += cfg.off_x;
    y += cfg.off_y;
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c)
            write_pixel(v, cfg, x + static_cast<int>(c),
                        y + static_cast<int>(r), rgb888);
//@LABS-STUB
    // BUG(2): the stamping loops are TRANSPOSED - rows iterate over the
    // WIDTH budget and columns over HEIGHT. A 4x2 rectangle therefore
    // paints 2 wide and 4 tall: sprites/UI elements come out rotated and
    // every frame hash diverges.
    w = copy_width(w);
    h = copy_height(h);
    x += cfg.off_x;
    y += cfg.off_y;
    for (uint32_t r = 0; r < w; ++r)       // wrong on purpose
        for (uint32_t c = 0; c < h; ++c)
            write_pixel(v, cfg, x + static_cast<int>(c),
                        y + static_cast<int>(r), rgb888);
//@LABS-END
}

// Twice the signed area in y-down screen space. Positive = clockwise on
// screen = front-facing (we render it); zero or negative is culled. This
// mirrors the GTE convention where games order front-facing polygons
// clockwise before handing them to the GPU.
inline int64_t signed_area2(const RasterVert& a, const RasterVert& b,
                            const RasterVert& c) {
    return static_cast<int64_t>(b.x - a.x) * (c.y - a.y) -
           static_cast<int64_t>(b.y - a.y) * (c.x - a.x);
}

// Top-left classification from an edge's direction (screen space). With the
// interior on the positive side of every edge (front-facing triangles), a
// boundary centre belongs to the primitive only along TOP edges
// (dy == 0 && dx > 0) or LEFT edges (dy > 0). Two triangles sharing an edge
// traverse it in the SAME direction, so they classify it identically —
// exactly one of them owns the boundary pixels.
inline constexpr bool is_top_left(int64_t dx, int64_t dy) {
    return dy > 0 || (dy == 0 && dx > 0);
}

namespace detail {

// One lattice edge function E(P) = cross(dir, P - origin); stepping one
// pixel adds step_x, stepping one row adds step_y.
struct Edge {
    int64_t value;
    int64_t step_x;
    int64_t step_y;
    bool include_zero;
};

inline Edge make_edge(int64_t ox, int64_t oy, int64_t dx, int64_t dy,
                      int64_t px, int64_t py) {
    Edge e;
    e.value = dx * (py - oy) - dy * (px - ox);
    e.step_x = -2 * dy;
    e.step_y = 2 * dx;
    e.include_zero = is_top_left(dx, dy);
    return e;
}

}  // namespace detail

// Q12 weighted channel resolve (task 7); declared early because the
// Gouraud rasterizer calls it.
inline uint32_t shade_channel(int64_t l0, int64_t l1, int64_t l2,
                              uint32_t c0, uint32_t c1, uint32_t c2);

// Flat-color triangle. All coordinates double into an integer lattice so
// pixel CENTRES (px+0.5, py+0.5) become exact points (2*px+1, 2*py+1).
// A centre is inside when every edge value is positive, or zero on an
// edge classified top/left.
inline void draw_triangle_flat(Vram& v, const DrawConfig& cfg,
                               uint32_t rgb888, const RasterVert& a,
                               const RasterVert& b, const RasterVert& c) {
    if (signed_area2(a, b, c) <= 0) return;  // backface cull

    const int xmin = imax(cfg.area_x1, imin(a.x, imin(b.x, c.x)));
    const int xmax = imin(cfg.area_x2, imax(a.x, imax(b.x, c.x)));
    const int ymin = imax(cfg.area_y1, imin(a.y, imin(b.y, c.y)));
    const int ymax = imin(cfg.area_y2, imax(a.y, imax(b.y, c.y)));
    if (xmin > xmax || ymin > ymax) return;

    const int64_t ax = 2 * a.x, ay = 2 * a.y;
    const int64_t bx = 2 * b.x, by = 2 * b.y;
    const int64_t cx = 2 * c.x, cy = 2 * c.y;
    detail::Edge e01 =
        detail::make_edge(ax, ay, bx - ax, by - ay, 2 * xmin + 1, 2 * ymin + 1);
    detail::Edge e12 =
        detail::make_edge(bx, by, cx - bx, cy - by, 2 * xmin + 1, 2 * ymin + 1);
    detail::Edge e20 =
        detail::make_edge(cx, cy, ax - cx, ay - cy, 2 * xmin + 1, 2 * ymin + 1);

    for (int y = ymin; y <= ymax; ++y) {
        int64_t v01 = e01.value, v12 = e12.value, v20 = e20.value;
        for (int x = xmin; x <= xmax; ++x) {
            const bool inside =
                (v01 > 0 || (v01 == 0 && e01.include_zero)) &&
                (v12 > 0 || (v12 == 0 && e12.include_zero)) &&
                (v20 > 0 || (v20 == 0 && e20.include_zero));
            if (inside) write_pixel(v, cfg, x, y, rgb888);
            v01 += e01.step_x;
            v12 += e12.step_x;
            v20 += e20.step_x;
        }
        e01.value += e01.step_y;
        e12.value += e12.step_y;
        e20.value += e20.step_y;
    }
}

// Gouraud triangle: same walk, but each pixel resolves per-channel color
// from barycentric weights in Q12 fixed point:
//   lambda_k = (E_edge_k << 12) / (4 * area2)      ; truncating division,
//                                                   ; all terms >= 0 here
//   channel  = (sum_k lambda_k * c_k + 2048) >> 12 ; round-half-up
// clamped to 0..255, then truncated to 5 bits when packed to 15bpp.
inline void draw_triangle_gouraud(Vram& v, const DrawConfig& cfg,
                                  const RasterVert& a, const RasterVert& b,
                                  const RasterVert& c) {
    const int64_t area2 = signed_area2(a, b, c);
    if (area2 <= 0) return;
    const int64_t den = 4 * area2;

    const int xmin = imax(cfg.area_x1, imin(a.x, imin(b.x, c.x)));
    const int xmax = imin(cfg.area_x2, imax(a.x, imax(b.x, c.x)));
    const int ymin = imax(cfg.area_y1, imin(a.y, imin(b.y, c.y)));
    const int ymax = imin(cfg.area_y2, imax(a.y, imax(b.y, c.y)));
    if (xmin > xmax || ymin > ymax) return;

    const int64_t ax = 2 * a.x, ay = 2 * a.y;
    const int64_t bx = 2 * b.x, by = 2 * b.y;
    const int64_t cx = 2 * c.x, cy = 2 * c.y;
    detail::Edge e01 =
        detail::make_edge(ax, ay, bx - ax, by - ay, 2 * xmin + 1, 2 * ymin + 1);
    detail::Edge e12 =
        detail::make_edge(bx, by, cx - bx, cy - by, 2 * xmin + 1, 2 * ymin + 1);
    detail::Edge e20 =
        detail::make_edge(cx, cy, ax - cx, ay - cy, 2 * xmin + 1, 2 * ymin + 1);

    for (int y = ymin; y <= ymax; ++y) {
        int64_t v01 = e01.value, v12 = e12.value, v20 = e20.value;
        for (int x = xmin; x <= xmax; ++x) {
            if ((v01 > 0 || (v01 == 0 && e01.include_zero)) &&
                (v12 > 0 || (v12 == 0 && e12.include_zero)) &&
                (v20 > 0 || (v20 == 0 && e20.include_zero))) {
                // Weight of vertex k comes from the edge OPPOSITE to k.
                const int64_t l0 = (v12 << 12) / den;  // weight of a
                const int64_t l1 = (v20 << 12) / den;  // weight of b
                const int64_t l2 = (v01 << 12) / den;  // weight of c
                const uint32_t r = shade_channel(l0, l1, l2, a.r, b.r, c.r);
                const uint32_t g = shade_channel(l0, l1, l2, a.g, b.g, c.g);
                const uint32_t bl = shade_channel(l0, l1, l2, a.b, b.b, c.b);
                write_pixel(v, cfg, x, y,
                            r | (g << 8) | (bl << 16));
            }
            v01 += e01.step_x;
            v12 += e12.step_x;
            v20 += e20.step_x;
        }
        e01.value += e01.step_y;
        e12.value += e12.step_y;
        e20.value += e20.step_y;
    }
}

inline uint32_t shade_channel(int64_t l0, int64_t l1, int64_t l2, uint32_t c0,
                              uint32_t c1, uint32_t c2) {
    int64_t ch = (l0 * static_cast<int64_t>(c0) +
                  l1 * static_cast<int64_t>(c1) +
                  l2 * static_cast<int64_t>(c2) + 2048) >> 12;
    if (ch < 0) ch = 0;
    if (ch > 255) ch = 255;
    return static_cast<uint32_t>(ch);
}

// ---------------------------------------------------------------------------
// Packet glue (provided): decode untextured polygon/rectangle payloads and
// rasterize them. prm[0] is the packet HEADER word (opcode<<24 | color);
// prm[1..] are the parameter words counted by gp0_param_words().
// ---------------------------------------------------------------------------
inline RasterVert vert_of(uint32_t word, const DrawConfig& cfg) {
    RasterVert v;
    v.x = sext11(word) + cfg.off_x;
    v.y = sext11(word >> 11) + cfg.off_y;
    return v;
}

inline RasterVert colored_vert_of(uint32_t word, uint32_t color,
                                  const DrawConfig& cfg) {
    RasterVert v = vert_of(word, cfg);
    v.r = color & 0xFF;
    v.g = (color >> 8) & 0xFF;
    v.b = (color >> 16) & 0xFF;
    return v;
}

inline void quad_as_tris(const RasterVert& v1, const RasterVert& v2,
                         const RasterVert& v3, const RasterVert& v4,
                         const auto& draw_fn) {
    // PSX-SPX: four-point polygons are processed as triangles (1,2,3) and
    // (2,3,4) — which also fixes the shared-edge ownership automatically.
    draw_fn(v1, v2, v3);
    draw_fn(v2, v3, v4);
}

inline void draw_render_packet(Vram& v, const DrawConfig& cfg, uint8_t cmd,
                               std::span<const uint32_t> prm) {
    const uint32_t color = prm[0] & 0x00FFFFFF;
    switch (cmd) {
        case 0x20: case 0x21: case 0x22: case 0x23: {   // mono tri
            draw_triangle_flat(v, cfg, color, vert_of(prm[1], cfg),
                               vert_of(prm[2], cfg), vert_of(prm[3], cfg));
            break;
        }
        case 0x28: case 0x29: case 0x2A: case 0x2B: {   // mono quad
            const RasterVert v1 = vert_of(prm[1], cfg);
            const RasterVert v2 = vert_of(prm[2], cfg);
            const RasterVert v3 = vert_of(prm[3], cfg);
            const RasterVert v4 = vert_of(prm[4], cfg);
            quad_as_tris(v1, v2, v3, v4, [&](const RasterVert& p,
                                             const RasterVert& q,
                                             const RasterVert& r) {
                draw_triangle_flat(v, cfg, color, p, q, r);
            });
            break;
        }
        case 0x30: case 0x31: case 0x32: case 0x33: {   // shaded tri
            const RasterVert v1 =
                colored_vert_of(prm[1], prm[0], cfg);
            const RasterVert v2 =
                colored_vert_of(prm[3], prm[2], cfg);
            const RasterVert v3 =
                colored_vert_of(prm[5], prm[4], cfg);
            draw_triangle_gouraud(v, cfg, v1, v2, v3);
            break;
        }
        case 0x38: case 0x39: case 0x3A: case 0x3B: {   // shaded quad
            const RasterVert v1 = colored_vert_of(prm[1], prm[0], cfg);
            const RasterVert v2 = colored_vert_of(prm[3], prm[2], cfg);
            const RasterVert v3 = colored_vert_of(prm[5], prm[4], cfg);
            const RasterVert v4 = colored_vert_of(prm[7], prm[6], cfg);
            quad_as_tris(v1, v2, v3, v4, [&](const RasterVert& p,
                                             const RasterVert& q,
                                             const RasterVert& r) {
                draw_triangle_gouraud(v, cfg, p, q, r);
            });
            break;
        }
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: case 0x66: case 0x67: {   // variable rect
            draw_rectangle(v, cfg, color,
                           sext11(prm[1]) + cfg.off_x,
                           sext11(prm[1] >> 11) + cfg.off_y,
                           prm[2] & 0xFFFF, prm[2] >> 16);
            break;
        }
        default: break;
    }
}

}  // namespace psx::gpu
