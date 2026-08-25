#pragma once
// Challenge GPU — complete GP0/GP1 pipeline: FIFO parsing, display control,
// VRAM transfers, FILL and untextured primitive rasterization. This is the
// reference implementation the exercises build toward; it carries no LABS
// markers on purpose.
#include "../shared/vram.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace psx::gpu {


// Sign-extend an 11-bit vertex coordinate field.
inline constexpr int sext11(uint32_t v) {
    v &= 0x7FFu;
    return (v & 0x400u) != 0u ? static_cast<int>(v) - 0x800
                              : static_cast<int>(v);
}

inline constexpr int imin(int a, int b) { return a < b ? a : b; }
inline constexpr int imax(int a, int b) { return a > b ? a : b; }

// Rendering attributes that constrain rasterization.
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


// Register-visible GPU attributes (the subset reachable in this chapter).
struct GpuRegs {
    // GP0(E1h) draw mode -> GPUSTAT.0-10,15
    uint32_t draw_mode = 0;
    // GP0(E2h) texture window (stored raw; consumed by ch42)
    uint32_t tex_window = 0;
    // GP0(E3h/E4h) drawing area corners (inclusive, absolute VRAM coords)
    int area_x1 = 0, area_y1 = 0;
    int area_x2 = kVramWidth - 1, area_y2 = kVramHeight - 1;
    // GP0(E5h) drawing offset added to every render vertex
    int off_x = 0, off_y = 0;
    // GP0(E6h) mask bits -> GPUSTAT.11-12
    bool set_mask = false;     // force bit15 on written pixels
    bool check_mask = false;   // skip pixels whose destination bit15=1
    // GP1 results
    bool display_off = true;   // GP1(03h); power-on default shows black
    uint32_t dma_dir = 0;      // GP1(04h) -> GPUSTAT.29-30
    uint32_t disp_x = 0, disp_y = 0;        // GP1(05h)
    uint32_t hrange_x1 = 0, hrange_x2 = 0;  // GP1(06h)
    uint32_t vrange_y1 = 0, vrange_y2 = 0;  // GP1(07h)
    uint32_t video_mode_bits = 0;           // GP1(08h) bits 0-7
    bool irq = false;                       // GPUSTAT.24 via GP0(1Fh)
};

// Project register state onto the rasterizer configuration.
inline DrawConfig render_config(const GpuRegs& regs) {
    DrawConfig c;
    c.area_x1 = regs.area_x1;
    c.area_y1 = regs.area_y1;
    c.area_x2 = regs.area_x2;
    c.area_y2 = regs.area_y2;
    c.off_x = regs.off_x;
    c.off_y = regs.off_y;
    c.set_mask = regs.set_mask;
    c.check_mask = regs.check_mask;
    return c;
}

// Rasterizer API (defined below the command layer).
inline void draw_rectangle(Vram&, const DrawConfig&, uint32_t, int, int,
                           uint32_t, uint32_t);
inline void draw_triangle_flat(Vram&, const DrawConfig&, uint32_t,
                               const RasterVert&, const RasterVert&,
                               const RasterVert&);
inline void draw_triangle_gouraud(Vram&, const DrawConfig&, const RasterVert&,
                                  const RasterVert&, const RasterVert&);
inline void draw_render_packet(Vram&, const DrawConfig&, uint8_t,
                               std::span<const uint32_t>);

class Gpu {
public:
    Vram vram;
    GpuRegs regs;

    void reset();
    bool write_gp0(uint32_t word);
    void write_gp1(uint32_t word);
    uint32_t status() const;
    uint32_t gpuread();
    size_t pending_words() const { return count_; }

private:
    static constexpr size_t kFifoWords = 16;  // 64-byte command FIFO

    std::array<uint32_t, kFifoWords> fifo_{};
    size_t head_ = 0, tail_ = 0, count_ = 0;

    enum class Mode { Idle, Params, CpuToVram };
    Mode mode_ = Mode::Idle;
    uint32_t cmd_ = 0;
    uint32_t params_left_ = 0;
    std::array<uint32_t, 12> prm_{};  // prm_[0]=header, prm_[1..]=parameters
    uint32_t prm_n_ = 0;

    // Streaming state for GP0(A0h)/(C0h)
    uint32_t xfer_left_ = 0;   // halfwords still expected from the CPU
    uint32_t xf_pos_ = 0;      // halfwords already written
    int xf_x_ = 0, xf_y_ = 0;
    uint32_t xf_w_ = 0;

    std::vector<uint16_t> read_fifo_;  // latched data for GPUREAD
    size_t read_pos_ = 0;

    void drain();
    void feed(uint32_t word);
    void write_stream_pixel(uint16_t px);
    void exec_current();
    void apply_copy_with_mask(std::span<const uint16_t> src, int dx, int dy,
                              uint32_t w, uint32_t h);
};

// Packet-length table: how many PARAMETER words follow each GP0 opcode
// (the header word itself is not counted). Correct lengths matter even for
// commands we do not render yet — a wrong count desynchronises the whole
// stream behind it. Poly-line commands (48h..5Ah) have variable length
// terminated by 55555555h and are out of scope here (length 0 => treated
// as one-word no-ops).
inline constexpr uint32_t gp0_param_words(uint8_t cmd) {
    if (cmd == 0x02) return 2;                        // FILL
    if (cmd == 0x80) return 3;                        // VRAM->VRAM: src,dst,wh
    if (cmd == 0xA0 || cmd == 0xC0) return 2;         // transfer headers
    if (cmd >= 0x20 && cmd <= 0x23) return 3;         // mono triangle
    if (cmd >= 0x28 && cmd <= 0x2B) return 4;         // mono quad
    if (cmd >= 0x24 && cmd <= 0x27) return 6;         // textured tri
    if (cmd >= 0x2C && cmd <= 0x2F) return 8;         // textured quad
    if (cmd >= 0x30 && cmd <= 0x33) return 5;         // shaded tri
    if (cmd >= 0x38 && cmd <= 0x3B) return 7;         // shaded quad
    if (cmd >= 0x34 && cmd <= 0x37) return 8;         // shaded-textured tri
    if (cmd >= 0x3C && cmd <= 0x3F) return 10;        // shaded-textured quad
    if (cmd >= 0x40 && cmd <= 0x47) return 2;         // mono line
    if (cmd >= 0x50 && cmd <= 0x57) return 4;         // shaded line
    if (cmd >= 0x60 && cmd <= 0x67) return 2;         // variable rect
    if (cmd >= 0x68 && cmd <= 0x7F) return 1;         // fixed-size rect
    return 0;  // 00h,01h,03h,1Fh,E1h..E6h and unknown opcodes
}

void Gpu::reset() {
    // Power-on GPUSTAT reads 14802000h: FIFO empty (bit28), ready for
    // commands (bit26), display disabled (bit23), interlace field high
    // (bit13). Reset does NOT clear VRAM.
    head_ = tail_ = count_ = 0;
    mode_ = Mode::Idle;
    cmd_ = params_left_ = prm_n_ = 0;
    xfer_left_ = xf_pos_ = xf_w_ = 0;
    xf_x_ = xf_y_ = 0;
    read_fifo_.clear();
    read_pos_ = 0;
    regs = GpuRegs{};
}

// Push one word into the 16-word FIFO and run the parser. In this chapter's
// zero-latency timing model the FIFO drains within the same call; it only
// becomes observable through GP1(01h) flushing a partially received packet.
bool Gpu::write_gp0(uint32_t word) {
    if (count_ >= kFifoWords) return false;  // stalled burst: word dropped
    fifo_[head_] = word;
    head_ = (head_ + 1) % kFifoWords;
    ++count_;
    drain();
    return true;
}

void Gpu::drain() {
    while (count_ > 0) {
        const uint32_t word = fifo_[tail_];
        tail_ = (tail_ + 1) % kFifoWords;
        --count_;
        feed(word);
    }
}

// GP1 display-control commands. PSX-SPX numbering: 00h reset, 01h reset
// command buffer, 02h acknowledge IRQ1, 03h display enable, 04h DMA
// direction, 05h/06h/07h display ranges, 08h video mode. Unknown opcodes
// change nothing (documented hardware behaviour).
void Gpu::write_gp1(uint32_t word) {
    switch (word >> 24) {
        case 0x00: {  // full reset: flush FIFO, abort packet, default regs
            const Vram keep = vram;  // reset does NOT clear VRAM
            reset();
            vram = keep;
            break;
        }
        case 0x01:  // reset command buffer: flush FIFO, abort current packet
            head_ = tail_ = count_ = 0;
            mode_ = Mode::Idle;
            params_left_ = prm_n_ = 0;
            xfer_left_ = 0;
            break;
        case 0x02: regs.irq = false; break;          // acknowledge IRQ1
        case 0x03: regs.display_off = (word & 1) != 0; break;
        case 0x04: regs.dma_dir = word & 3; break;
        case 0x05:
            regs.disp_x = word & 0x3FF;
            regs.disp_y = (word >> 10) & 0x1FF;
            break;
        case 0x06:
            regs.hrange_x1 = word & 0xFFF;
            regs.hrange_x2 = (word >> 12) & 0xFFF;
            break;
        case 0x07:
            regs.vrange_y1 = word & 0xFFF;
            regs.vrange_y2 = (word >> 12) & 0xFFF;
            break;
        case 0x08: regs.video_mode_bits = word & 0xFF; break;
        default: break;  // 09h/10h/20h exist but carry no visible state here
    }
}

// The packet state machine. Idle: a word is a header — latch opcode AND the
// word itself into prm_[0], then queue its parameter count. Params: append
// words at prm_[1..]; when the last one arrives, execute. CpuToVram: every
// word carries two halfwords of image data until xfer_left_ reaches zero
// (an odd total takes the low halfword of the final word).
void Gpu::feed(uint32_t word) {
    switch (mode_) {
        case Mode::Idle: {
            cmd_ = static_cast<uint8_t>(word >> 24);
            prm_n_ = 1;
            prm_[0] = word;  // header: color+cmd for FILL, args for E1h..E6h
            params_left_ = gp0_param_words(cmd_);
            mode_ = params_left_ > 0 ? Mode::Params : Mode::Idle;
            if (params_left_ == 0) exec_current();
            break;
        }
        case Mode::Params:
            prm_[prm_n_++] = word;
            if (--params_left_ == 0) {
                mode_ = Mode::Idle;
                exec_current();
            }
            break;
        case Mode::CpuToVram:
            for (uint32_t half = 0; half < 2 && xfer_left_ > 0; ++half) {
                const uint16_t px =
                    static_cast<uint16_t>((word >> (16 * half)) & 0xFFFF);
                write_stream_pixel(px);
            }
            if (xfer_left_ == 0) mode_ = Mode::Idle;
            break;
    }
}

void Gpu::write_stream_pixel(uint16_t px) {
    // Destination addressing wraps per pixel: column modulo 1024 within
    // the same row, row modulo 512 — never a carry from X into Y.
    const int dx = xf_x_ + static_cast<int>(xf_pos_ % xf_w_);
    const int dy = xf_y_ + static_cast<int>(xf_pos_ / xf_w_);
    ++xf_pos_;
    --xfer_left_;
    uint16_t& dst = vram.at(dx, dy);
    if (regs.check_mask && (dst & 0x8000u) != 0u) return;
    // Transfers honour both mask bits like rendering does (PSX-SPX); for
    // raw halfword data "set mask" forces bit15 of the written value.
    dst = px | (regs.set_mask ? 0x8000u : 0x0000u);
}

// Execute a fully received GP0 packet. Rendering commands are decoded with
// correct lengths by gp0_param_words() but draw nothing here yet; FILL and
// the three VRAM-access commands do their real work.
void Gpu::exec_current() {
    switch (cmd_) {
        case 0x00: break;  // nop
        case 0x01: break;  // clear texture cache (no cache modelled)
        case 0x03: break;  // timing control (no timing modelled)
        case 0x1F: regs.irq = true; break;  // request IRQ1 -> GPUSTAT.24
        case 0x02: {   // FILL: header carries the color; PSX-SPX parameter
                       // masking lives in fill_vram()
            const uint16_t color = rgb888_to_bgr555(prm_[0] & 0x00FFFFFFu);
            fill_vram(vram, color, prm_[1] & 0xFFFF, prm_[1] >> 16,
                      prm_[2] & 0xFFFF, prm_[2] >> 16);
            break;
        }
        case 0x80: {   // VRAM->VRAM copy (snapshot source, mask-aware write)
            const uint32_t w = copy_width(prm_[3] & 0xFFFF);
            const uint32_t h = copy_height(prm_[3] >> 16);
            const std::vector<uint16_t> snap =
                vram_to_cpu(vram, static_cast<int>(prm_[1] & 0x3FF),
                            static_cast<int>((prm_[1] >> 10) & 0x1FF), w, h);
            apply_copy_with_mask(snap, static_cast<int>(prm_[2] & 0x3FF),
                                 static_cast<int>((prm_[2] >> 10) & 0x1FF),
                                 w, h);
            break;
        }
        case 0xA0: {   // CPU->VRAM: switch the port into streaming mode
            xf_x_ = static_cast<int>(prm_[1] & 0x3FF);
            xf_y_ = static_cast<int>((prm_[1] >> 10) & 0x1FF);
            xf_w_ = copy_width(prm_[2] & 0xFFFF);
            xfer_left_ = xf_w_ * copy_height(prm_[2] >> 16);
            xf_pos_ = 0;
            mode_ = Mode::CpuToVram;
            break;
        }
        case 0xC0: {   // VRAM->CPU: latch data; GPUREAD pops it as words
            read_fifo_ =
                vram_to_cpu(vram, static_cast<int>(prm_[1] & 0x3FF),
                            static_cast<int>((prm_[1] >> 10) & 0x1FF),
                            copy_width(prm_[2] & 0xFFFF),
                            copy_height(prm_[2] >> 16));
            if (read_fifo_.size() % 2 != 0)
                read_fifo_.push_back(0);  // extra halfword on odd sizes
            read_pos_ = 0;
            break;
        }
        case 0xE1: regs.draw_mode = prm_[0]; break;
        case 0xE2: regs.tex_window = prm_[0] & 0xFFFFFu; break;
        case 0xE3:
            regs.area_x1 = static_cast<int>(prm_[0] & 0x3FF);
            regs.area_y1 = static_cast<int>((prm_[0] >> 10) & 0x1FF);
            break;
        case 0xE4:
            regs.area_x2 = static_cast<int>(prm_[0] & 0x3FF);
            regs.area_y2 = static_cast<int>((prm_[0] >> 10) & 0x1FF);
            break;
        case 0xE5:
            regs.off_x = sext11(prm_[0]);
            regs.off_y = sext11(prm_[0] >> 11);
            break;
        case 0xE6:
            regs.set_mask = (prm_[0] & 1) != 0;
            regs.check_mask = (prm_[0] & 2) != 0;
            break;
        default: {
            // Rendering commands: untextured polygons + variable/fixed
            // rectangles. Textured primitives decode correctly (word
            // counts) but draw nothing until ch42.
            const std::span<const uint32_t> args(prm_.data(), 12);
            if ((cmd_ >= 0x20 && cmd_ <= 0x3F) ||
                (cmd_ >= 0x60 && cmd_ <= 0x67)) {
                draw_render_packet(vram, render_config(regs), cmd_, args);
            } else if (cmd_ >= 0x68 && cmd_ <= 0x7F) {
                static constexpr uint32_t kFixed[3] = {1, 8, 16};  // 68/70/78h
                const uint32_t size = kFixed[((cmd_ >> 3) & 3) - 1];
                const int x = sext11(prm_[1]) + regs.off_x;
                const int y = sext11(prm_[1] >> 11) + regs.off_y;
                draw_rectangle(vram, render_config(regs),
                               prm_[0] & 0x00FFFFFFu, x, y, size, size);
            }
            break;
        }
    }
}

// Mask-aware scatter of a snapshot copy to (dx,dy).
void Gpu::apply_copy_with_mask(std::span<const uint16_t> src, int dx, int dy,
                               uint32_t w, uint32_t h) {
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c) {
            uint16_t& dst = vram.at(dx + static_cast<int>(c),
                                    dy + static_cast<int>(r));
            if (regs.check_mask && (dst & 0x8000u) != 0u) continue;
            dst = src[r * w + c] | (regs.set_mask ? 0x8000u : 0x0000u);
        }
}

// GPUREAD (reads of 1F801810h): pop two latched halfwords assembled
// little-endian; an odd-sized download ends with one lone halfword padded
// into a full word ("an extra halfword is read at the end" — PSX-SPX).
uint32_t Gpu::gpuread() {
    if (read_pos_ >= read_fifo_.size()) return 0;
    const uint32_t lo = read_fifo_[read_pos_++];
    uint32_t hi = 0;
    if (read_pos_ < read_fifo_.size()) hi = read_fifo_[read_pos_++];
    return lo | (hi << 16);
}

// GPUSTAT bit layout (PSX-SPX "GPU Status Register"). Power-on value is
// 14802000h. DRQ semantics depend on the GP1(04h) direction: mode 0 keeps
// bit25 low, mode 1 requests while the FIFO is at least half empty, mode 2
// mirrors "FIFO empty" (bit28), mode 3 mirrors "read FIFO has data"
// (bit27).
uint32_t Gpu::status() const {
    uint32_t s = 0;
    s |= (regs.draw_mode & 0xF);                       // 0-3 texpage X
    s |= ((regs.draw_mode >> 4) & 1) << 4;             // 4 texpage Y
    s |= ((regs.draw_mode >> 5) & 3) << 5;             // 5-6 semi-transp
    s |= ((regs.draw_mode >> 7) & 3) << 7;             // 7-8 tex colors
    s |= ((regs.draw_mode >> 9) & 1) << 9;             // 9 dither
    s |= ((regs.draw_mode >> 10) & 1) << 10;           // 10 interlace draw
    s |= (regs.set_mask ? 1u : 0u) << 11;              // 11
    s |= (regs.check_mask ? 1u : 0u) << 12;            // 12
    s |= 1u << 13;   // 13 interlace field: always 1 in progressive mode
    s |= ((regs.video_mode_bits >> 7) & 1) << 14;      // 14 reverse flag
    s |= ((regs.draw_mode >> 11) & 1) << 15;           // 15 texpage Y bit9
    s |= ((regs.video_mode_bits >> 6) & 1) << 16;      // 16 hres2 (368)
    s |= (regs.video_mode_bits & 3) << 17;             // 17-18 hres1
    s |= ((regs.video_mode_bits >> 2) & 1) << 19;      // 19 vres
    s |= ((regs.video_mode_bits >> 3) & 1) << 20;      // 20 NTSC/PAL
    s |= ((regs.video_mode_bits >> 4) & 1) << 21;      // 21 15/24bit
    s |= ((regs.video_mode_bits >> 5) & 1) << 22;      // 22 interlace
    s |= (regs.display_off ? 1u : 0u) << 23;           // 23 display enable
    s |= (regs.irq ? 1u : 0u) << 24;                   // 24 IRQ1
    bool drq = false;                                  // 25 DMA request
    if (regs.dma_dir == 1) drq = count_ <= kFifoWords / 2;
    else if (regs.dma_dir == 2) drq = count_ == 0;
    else if (regs.dma_dir == 3) drq = read_pos_ < read_fifo_.size();
    s |= (drq ? 1u : 0u) << 25;
    s |= (count_ < kFifoWords ? 1u : 0u) << 26;        // 26 ready for cmd
    s |= (read_pos_ < read_fifo_.size() ? 1u : 0u) << 27;  // 27 data avail
    s |= (count_ == 0 ? 1u : 0u) << 28;                // 28 FIFO empty
    s |= (regs.dma_dir & 3) << 29;                     // 29-30 DMA dir
    return s;                                          // 31 even line / vbl
}

// Final pixel write: clip against the drawing area, honour the check-mask
// (destination bit15 write-protects the pixel) and force bit15 when the
// set-mask bit is on. Untextured pixels otherwise store bit15=0.
inline void write_pixel(Vram& v, const DrawConfig& cfg, int x, int y,
                        uint32_t rgb888) {
    if (x < cfg.area_x1 || x > cfg.area_x2 || y < cfg.area_y1 ||
        y > cfg.area_y2)
        return;
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
    w = copy_width(w);
    h = copy_height(h);
    x += cfg.off_x;
    y += cfg.off_y;
    for (uint32_t r = 0; r < h; ++r)
        for (uint32_t c = 0; c < w; ++c)
            write_pixel(v, cfg, x + static_cast<int>(c),
                        y + static_cast<int>(r), rgb888);
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
