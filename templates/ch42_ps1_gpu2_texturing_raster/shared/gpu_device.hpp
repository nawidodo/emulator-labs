#pragma once
// ch42 shared GPU device — GP0 command-stream parser, primitive setup and the
// software rasterizer skeleton. This file contains NO @LABS blocks: the four
// rendering stages live in per-exercise "Stages" structs and are invoked as
//
//     primitive_setup -> raster/blit loops -> S::texture_fetch
//                                         -> S::transparency_skip
//                                         -> S::mask_test
//                                         -> S::blend_pixel
//
// so every solution visibly separates the stages the curriculum asks for.
// Conventions follow PSX-SPX: polygons exclude their lower-right boundary
// (top-left fill rule), rectangles include it, rects are never dithered, and
// the written bit15 obeys GP0(E6h).
#include <cstdint>
#include <vector>

#include "gpu_state.hpp"

namespace psx::gpu {

// One prepared (screen-space) vertex: position has the drawing offset applied,
// UVs stay in whole texels until the barycentric interpolation.
struct RawVert {
    int x, y, u, v;
};

struct SetupVert {
    int x, y, u, v;
};

using RawTri = RawVert[3];

// Directed-edge function; positive on the interior after the winding
// normalization in rasterize below.
inline long long edge_fn(const SetupVert& a, const SetupVert& b, int x, int y) {
    return static_cast<long long>(b.x - a.x) * (y - a.y) -
           static_cast<long long>(b.y - a.y) * (x - a.x);
}

// Top-left rule for a directed edge (y-down screen, positive-area windings):
// horizontal edges count as "top" when traversed left->right, everything else
// counts as "left" when traversed downward. Reversed traversals negate the
// predicate, so two triangles sharing an edge agree exactly.
inline bool edge_top_left(const SetupVert& a, const SetupVert& b) {
    return (a.y == b.y) ? (a.x < b.x) : (b.y > a.y);
}

// ---------------------------------------------------------------------------
// Stage 1: primitive_setup — applies the GP0(E5h) drawing offset BEFORE any
// clipping happens (hardware semantics; the offset shifts primitives into or
// out of the drawing area, it is not a translation after clipping).
// ---------------------------------------------------------------------------
template <class S>
inline void primitive_setup(const RawTri& raw, int off_x, int off_y,
                            SetupVert (&out)[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i].x = S::screen_coord(raw[i].x, off_x);
        out[i].y = S::screen_coord(raw[i].y, off_y);
        out[i].u = raw[i].u;
        out[i].v = raw[i].v;
    }
}

// ---------------------------------------------------------------------------
// Per-pixel pipeline shared by every primitive shape. The stages themselves
// come from the exercise's Stages struct.
// ---------------------------------------------------------------------------
template <class S>
inline void plot_textured(Vram& vram, const TexEnv& env, const PrimCtx& ctx,
                          int x, int y, int uf8, int vf8) {
    if (!S::in_draw_area(x, y, ctx.area)) return;          // stage: clip
    const uint16_t texel = S::texture_fetch(env, uf8, vf8);  // stage: fetch
    if (S::transparency_skip(texel, ctx)) return;            // stage: STP rule
    const uint16_t bg = vram.at(x, y);
    if (S::mask_test(bg, ctx)) return;                       // stage: mask
    const uint16_t out = S::blend_pixel(bg, texel, ctx, x, y);  // stage: blend
    // Write epilogue: bit15 policy per GP0(E6h)/GPUSTAT — forced when
    // SetMask-bit is on, otherwise the texture's own STP flag survives.
    const uint16_t top =
        ctx.mask.set_bit ? static_cast<uint16_t>(0x8000)
                         : static_cast<uint16_t>(texel & 0x8000);
    vram.at(x, y) = static_cast<uint16_t>(top | (out & 0x7FFF));
}

template <class S>
inline void plot_flat(Vram& vram, const PrimCtx& ctx, int x, int y) {
    if (!S::in_draw_area(x, y, ctx.area)) return;
    const uint16_t bg = vram.at(x, y);
    if (S::mask_test(bg, ctx)) return;
    // Untextured primitives carry their colour directly; there is no texel,
    // hence no transparent-black rule and never a surviving STP flag.
    const uint16_t out = S::blend_pixel(bg, ctx.shade15, ctx, x, y);
    const uint16_t top = ctx.mask.set_bit ? static_cast<uint16_t>(0x8000) : 0;
    vram.at(x, y) = static_cast<uint16_t>(top | (out & 0x7FFF));
}

// ---------------------------------------------------------------------------
// Stage 2: rasterizers. Integer edge-function walk over the pixel bounding
// box; UVs interpolate screen-linearly in 8.8 fixed point (the PSX has no
// perspective correction).
// ---------------------------------------------------------------------------
template <class S>
inline void raster_textured_tri(Vram& vram, const TexEnv& env, const PrimCtx& ctx,
                                const SetupVert (&sv)[3]) {
    SetupVert a = sv[0], b = sv[1], c = sv[2];
    long long area = edge_fn(a, b, c.x, c.y);
    if (area == 0) return;  // degenerate: hardware draws nothing
    if (area < 0) {
        SetupVert t = b;
        b = c;
        c = t;
        area = -area;
    }
    int minx = a.x < b.x ? a.x : b.x;
    minx = minx < c.x ? minx : c.x;
    int maxx = a.x > b.x ? a.x : b.x;
    maxx = maxx > c.x ? maxx : c.x;
    int miny = a.y < b.y ? a.y : b.y;
    miny = miny < c.y ? miny : c.y;
    int maxy = a.y > b.y ? a.y : b.y;
    maxy = maxy > c.y ? maxy : c.y;

    int x0 = minx > ctx.area.x1 ? minx : ctx.area.x1;
    int x1 = maxx < ctx.area.x2 ? maxx : ctx.area.x2;
    int y0 = miny > ctx.area.y1 ? miny : ctx.area.y1;
    int y1 = maxy < ctx.area.y2 ? maxy : ctx.area.y2;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const long long w0 = edge_fn(b, c, x, y);
            const long long w1 = edge_fn(c, a, x, y);
            const long long w2 = edge_fn(a, b, x, y);
            const bool inside =
                (w0 > 0 || (w0 == 0 && edge_top_left(b, c))) &&
                (w1 > 0 || (w1 == 0 && edge_top_left(c, a))) &&
                (w2 > 0 || (w2 == 0 && edge_top_left(a, b)));
            if (!inside) continue;
            // Screen-linear UV interpolation, rounded to nearest in 8.8.
            const int uf8 = static_cast<int>(
                ((((w0 * static_cast<long long>(a.u)) << 8) +
                  ((w1 * static_cast<long long>(b.u)) << 8) +
                  ((w2 * static_cast<long long>(c.u)) << 8) + area / 2) /
                 area));
            const int vf8 = static_cast<int>(
                ((((w0 * static_cast<long long>(a.v)) << 8) +
                  ((w1 * static_cast<long long>(b.v)) << 8) +
                  ((w2 * static_cast<long long>(c.v)) << 8) + area / 2) /
                 area));
            plot_textured<S>(vram, env, ctx, x, y, uf8, vf8);
        }
    }
}

template <class S>
inline void raster_flat_tri(Vram& vram, const PrimCtx& ctx,
                            const SetupVert (&sv)[3]) {
    SetupVert a = sv[0], b = sv[1], c = sv[2];
    long long area = edge_fn(a, b, c.x, c.y);
    if (area == 0) return;
    if (area < 0) {
        SetupVert t = b;
        b = c;
        c = t;
        area = -area;
    }
    int minx = a.x < b.x ? a.x : b.x;
    minx = minx < c.x ? minx : c.x;
    int maxx = a.x > b.x ? a.x : b.x;
    maxx = maxx > c.x ? maxx : c.x;
    int miny = a.y < b.y ? a.y : b.y;
    miny = miny < c.y ? miny : c.y;
    int maxy = a.y > b.y ? a.y : b.y;
    maxy = maxy > c.y ? maxy : c.y;

    int x0 = minx > ctx.area.x1 ? minx : ctx.area.x1;
    int x1 = maxx < ctx.area.x2 ? maxx : ctx.area.x2;
    int y0 = miny > ctx.area.y1 ? miny : ctx.area.y1;
    int y1 = maxy < ctx.area.y2 ? maxy : ctx.area.y2;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const long long w0 = edge_fn(b, c, x, y);
            const long long w1 = edge_fn(c, a, x, y);
            const long long w2 = edge_fn(a, b, x, y);
            const bool inside =
                (w0 > 0 || (w0 == 0 && edge_top_left(b, c))) &&
                (w1 > 0 || (w1 == 0 && edge_top_left(c, a))) &&
                (w2 > 0 || (w2 == 0 && edge_top_left(a, b)));
            if (!inside) continue;
            plot_flat<S>(vram, ctx, x, y);
        }
    }
}

// Rectangles include their lower-right pixel (unlike polygons) and are
// clipped to the drawing area, but never dithered.
template <class S>
inline void blit_textured_rect(Vram& vram, const TexEnv& env, const PrimCtx& ctx,
                               int x, int y, int w, int h, int u0, int v0) {
    if (w <= 0 || h <= 0) return;
    const int xs = x > ctx.area.x1 ? x : ctx.area.x1;
    const int xe = (x + w - 1) < ctx.area.x2 ? (x + w - 1) : ctx.area.x2;
    const int ys = y > ctx.area.y1 ? y : ctx.area.y1;
    const int ye = (y + h - 1) < ctx.area.y2 ? (y + h - 1) : ctx.area.y2;
    for (int py = ys; py <= ye; ++py) {
        for (int px = xs; px <= xe; ++px) {
            plot_textured<S>(vram, env, ctx, px, py, (u0 + px - x) << 8,
                             (v0 + py - y) << 8);
        }
    }
}

template <class S>
inline void blit_flat_rect(Vram& vram, const PrimCtx& ctx, int x, int y, int w,
                           int h) {
    if (w <= 0 || h <= 0) return;
    const int xs = x > ctx.area.x1 ? x : ctx.area.x1;
    const int xe = (x + w - 1) < ctx.area.x2 ? (x + w - 1) : ctx.area.x2;
    const int ys = y > ctx.area.y1 ? y : ctx.area.y1;
    const int ye = (y + h - 1) < ctx.area.y2 ? (y + h - 1) : ctx.area.y2;
    for (int py = ys; py <= ye; ++py)
        for (int px = xs; px <= xe; ++px) plot_flat<S>(vram, ctx, px, py);
}

// ---------------------------------------------------------------------------
// GP0 command-stream device. feed() consumes one little-endian 32-bit word at
// a time, exactly what the --rom fixtures contain.
// ---------------------------------------------------------------------------
template <class Stages>
class GpuDevice {
public:
    Vram vram;
    DrawMode mode;
    TexWindow win;
    Clut clut;
    DrawArea area{};          // full VRAM until GP0(E3h/E4h) say otherwise
    int off_x = 0, off_y = 0;
    MaskSetting mask;

    struct CmdLogEntry {
        uint32_t pc;    // byte offset of the command's first word
        uint32_t word;  // the first word itself
    };
    std::vector<CmdLogEntry> cmd_log;

    void feed(uint32_t word, uint32_t pc_byte) {
        if (transfer_pos_ < transfer_total_) {
            feed_transfer_word(word);
            return;
        }
        if (need_ == 0) {
            cmd_log.push_back({pc_byte, word});
            start_command(word);
        } else {
            buf_.push_back(word);
            if (static_cast<int>(buf_.size()) == need_) finish_buffered();
        }
    }

    bool busy() const {
        return need_ != 0 || transfer_pos_ < transfer_total_;
    }

private:
    enum class Kind : uint8_t {
        Fill,
        MonoTri,
        MonoQuad,
        TexTri,
        TexQuad,
        FlatRect,
        TexRect,
        UploadHdr,
    };

    Kind kind_{};
    int need_ = 0;
    uint32_t cmd_word_ = 0;
    std::vector<uint32_t> buf_;

    // CPU->VRAM transfer state
    int tx_ = 0, ty_ = 0, tw_ = 0, th_ = 0, row_hw_ = 0;
    long long transfer_pos_ = 0, transfer_total_ = 0;

    void begin(Kind k, int more_words, uint32_t cmd) {
        kind_ = k;
        need_ = more_words;
        cmd_word_ = cmd;
        buf_.clear();
    }

    void start_command(uint32_t w) {
        const uint8_t cmd = static_cast<uint8_t>(w >> 24);
        switch (cmd) {
            case 0x02: begin(Kind::Fill, 3, w); break;
            case 0x20: case 0x22: begin(Kind::MonoTri, 3, w); break;
            case 0x28: case 0x2A: begin(Kind::MonoQuad, 4, w); break;
            case 0x24: case 0x25: case 0x26: case 0x27:
                begin(Kind::TexTri, 6, w);
                break;
            case 0x2C: case 0x2D: case 0x2E: case 0x2F:
                begin(Kind::TexQuad, 8, w);
                break;
            case 0xA0: begin(Kind::UploadHdr, 2, w); break;
            default:
                if (cmd >= 0xE1 && cmd <= 0xE6) {
                    exec_attr(w);
                } else if (cmd >= 0x60 && cmd <= 0x7F) {
                    const bool tex = (w & 0x04000000) != 0;
                    const int size = static_cast<int>((w >> 27) & 3);
                    // Variable-size rects carry W/H; textured ones also
                    // carry the texcoord+CLUT word. Fixed-size textured
                    // rects still carry texcoord+CLUT (no W/H word).
                    const int more =
                        size == 0 ? (tex ? 3 : 2) : (tex ? 2 : 1);
                    begin(tex ? Kind::TexRect : Kind::FlatRect, more, w);
                }
                // Lines, shaded polygons, VRAM copies: out of scope for ch42.
                break;
        }
    }

    void finish_buffered() {
        switch (kind_) {
            case Kind::Fill: exec_fill(buf_[0], buf_[1], buf_[2]); break;
            case Kind::MonoTri: exec_mono_poly(false); break;
            case Kind::MonoQuad: exec_mono_poly(true); break;
            case Kind::TexTri: exec_tex_poly(false); break;
            case Kind::TexQuad: exec_tex_poly(true); break;
            case Kind::FlatRect: exec_flat_rect(); break;
            case Kind::TexRect: exec_tex_rect(); break;
            case Kind::UploadHdr: start_upload(buf_[0], buf_[1]); break;
        }
        need_ = 0;
        buf_.clear();
    }

    void exec_attr(uint32_t w) {
        switch (w >> 24) {
            case 0xE1: decode_draw_mode(w, mode); break;
            case 0xE2: decode_tex_window(w, win); break;
            case 0xE3: decode_area_corner(w, area.x1, area.y1); break;
            case 0xE4: decode_area_corner(w, area.x2, area.y2); break;
            case 0xE5: decode_draw_offset(w, off_x, off_y); break;
            case 0xE6: decode_mask_setting(w, mask); break;
            default: break;
        }
    }

    PrimCtx make_ctx(bool textured) {
        PrimCtx ctx;
        ctx.shade_r = static_cast<int>(cmd_word_ & 0xFF);
        ctx.shade_g = static_cast<int>((cmd_word_ >> 8) & 0xFF);
        ctx.shade_b = static_cast<int>((cmd_word_ >> 16) & 0xFF);
        ctx.shade15 = rgb8_to_bgr15(ctx.shade_r, ctx.shade_g, ctx.shade_b);
        ctx.textured = textured;
        ctx.raw = (cmd_word_ & 0x01000000) != 0;
        ctx.semi = (cmd_word_ & 0x02000000) != 0;
        ctx.semi_mode = mode.semi;
        ctx.mask = mask;
        ctx.area = area;
        return ctx;
    }

    // GP0(02h): fills ignore the drawing area, offset, mask and dither; the
    // parameters round per PSX-SPX (16-halfword X steps).
    void exec_fill(uint32_t color_w, uint32_t xy_w, uint32_t wh_w) {
        const uint16_t c = rgb8_to_bgr15(static_cast<int>(color_w & 0xFF),
                                         static_cast<int>((color_w >> 8) & 0xFF),
                                         static_cast<int>((color_w >> 16) & 0xFF));
        const int x = static_cast<int>(xy_w & 0x3F0);
        const int y = static_cast<int>((xy_w >> 16) & 0x1FF);
        const int w = static_cast<int>(((wh_w & 0x3FF) + 0xF) & ~0xFu);
        const int h = static_cast<int>((wh_w >> 16) & 0x1FF);
        for (int py = y; py < y + h; ++py)
            for (int px = x; px < x + w; ++px) vram.at(px, py) = c;
    }

    void start_upload(uint32_t xy_w, uint32_t wh_w) {
        tx_ = static_cast<int>(xy_w & 0x3FF);
        ty_ = static_cast<int>((xy_w >> 16) & 0x1FF);
        tw_ = static_cast<int>(((wh_w & 0x3FF) - 1u) & 0x3FF) + 1;
        th_ = static_cast<int>((((wh_w >> 16) & 0x1FF) - 1u) & 0x1FF) + 1;
        row_hw_ = (tw_ + 1) / 2;      // rows pad to whole 32-bit words
        transfer_pos_ = 0;
        // Position and total are counted in HALFWORDS (two per word).
        transfer_total_ = static_cast<long long>(row_hw_) * 2 * th_;
    }

    void feed_transfer_word(uint32_t w) {
        for (int lane = 0; lane < 2 && transfer_pos_ < transfer_total_; ++lane) {
            const uint16_t src = static_cast<uint16_t>(
                lane == 0 ? (w & 0xFFFF) : ((w >> 16) & 0xFFFF));
            const int r = static_cast<int>(transfer_pos_ / (row_hw_ * 2));
            const int c = static_cast<int>(transfer_pos_ % (row_hw_ * 2));
            ++transfer_pos_;
            if (c >= tw_) continue;  // row padding halfword
            uint16_t& dst = vram.at(tx_ + c, ty_ + r);
            // Transfers honour the mask setting per halfword (but not the
            // drawing area, offset or transparency rules).
            if (mask.test_bit && (dst & 0x8000)) continue;
            dst = mask.set_bit ? static_cast<uint16_t>(src | 0x8000) : src;
        }
    }

    void exec_mono_poly(bool quad) {
        PrimCtx ctx = make_ctx(/*textured=*/false);
        RawVert rv[4];
        int n = quad ? 4 : 3;
        for (int i = 0; i < n; ++i)
            decode_vertex(buf_[i], rv[i].x, rv[i].y);
        draw_raw_poly(ctx, rv, n, nullptr);
    }

    void exec_tex_poly(bool quad) {
        // Vertex 2's texcoord word carries the Texpage attribute, which
        // refreshes GP0(E1h) bits 0-8 (dither survives; only E1h sets it).
        merge_texpage_attr(buf_[3], mode);
        PrimCtx ctx = make_ctx(/*textured=*/true);
        TexEnv env;
        env.vram = &vram;
        env.mode = mode;
        env.win = win;
        env.clut = decode_clut(buf_[1]);
        ctx.dither = !ctx.raw && mode.dither;

        RawVert rv[4];
        int n = quad ? 4 : 3;
        const uint32_t* p = buf_.data();
        for (int i = 0; i < n; ++i) {
            decode_vertex(p[2 * i], rv[i].x, rv[i].y);
            rv[i].u = static_cast<int>(p[2 * i + 1] & 0xFF);
            rv[i].v = static_cast<int>((p[2 * i + 1] >> 8) & 0xFF);
        }
        draw_raw_poly(ctx, rv, n, &env);
    }

    // Shared by mono/textured, tri/quad. Quads split into vertices 1-2-3 and
    // 2-3-4, matching the hardware's internal handling.
    void draw_raw_poly(const PrimCtx& base_ctx, const RawVert* rv, int n,
                       const TexEnv* env) {
        auto draw_one = [&](const RawVert (&tri)[3]) {
            SetupVert sv[3];
            primitive_setup<Stages>(tri, off_x, off_y, sv);
            if (env != nullptr)
                raster_textured_tri<Stages>(vram, *env, base_ctx, sv);
            else
                raster_flat_tri<Stages>(vram, base_ctx, sv);
        };
        RawTri t;
        t[0] = rv[0];
        t[1] = rv[1];
        t[2] = rv[2];
        draw_one(t);
        if (n == 4) {
            t[0] = rv[1];
            t[1] = rv[2];
            t[2] = rv[3];
            draw_one(t);
        }
    }

    void exec_flat_rect() {
        PrimCtx ctx = make_ctx(false);
        const int x = sext11(buf_[0]);
        const int y = sext11(buf_[0] >> 16);
        int w, h;
        rect_size(buf_.size() > 1 ? buf_[1] : 0, w, h);
        blit_flat_rect<Stages>(vram, ctx, x, y, w, h);
    }

    void exec_tex_rect() {
        PrimCtx ctx = make_ctx(true);
        // Unlike textured polygons, rectangles take the Texpage exclusively
        // from GP0(E1h) state — there is no per-primitive texpage attribute.
        ctx.dither = false;  // rects are NEVER dithered (PSX-SPX)
        TexEnv env;
        env.vram = &vram;
        env.mode = mode;
        env.win = win;
        env.clut = decode_clut(buf_[1]);
        const int x = sext11(buf_[0]);
        const int y = sext11(buf_[0] >> 16);
        const int u0 = static_cast<int>(buf_[1] & 0xFF);
        const int v0 = static_cast<int>((buf_[1] >> 8) & 0xFF);
        int w, h;
        if (((cmd_word_ >> 27) & 3) == 0) {
            rect_size(buf_[2], w, h);
        } else {
            static const int kFixed[4] = {0, 1, 8, 16};
            w = h = kFixed[(cmd_word_ >> 27) & 3];
        }
        blit_textured_rect<Stages>(vram, env, ctx, x, y, w, h, u0, v0);
    }

    static void rect_size(uint32_t wh_w, int& w, int& h) {
        w = static_cast<int>(((wh_w & 0x3FF) - 1u) & 0x3FF) + 1;
        h = static_cast<int>(((wh_w >> 16) & 0x1FF) - 1u & 0x1FF) + 1;
    }
};

}  // namespace psx::gpu
