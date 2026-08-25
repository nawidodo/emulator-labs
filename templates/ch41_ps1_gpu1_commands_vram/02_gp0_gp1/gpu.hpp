#pragma once
// Exercise 02 — GP0/GP1 command FIFO, GPUSTAT and the FILL command.
//
// The CPU talks to the GPU through two ports: GP0 (1F801810h) carries
// rendering/VRAM packets, GP1 (1F801814h) carries display-control commands.
// GP0 words enter a 64-byte (16-word) hardware FIFO; a packet is a one-word
// header (opcode in bits 24-31) followed by a command-specific number of
// parameter words. Polygon/line execution already starts when the first
// vertices arrive; block transfers switch the port into a streaming mode
// until w*h halfwords have moved.
//
// Parameter indexing: prm_[0] ALWAYS holds the header word itself — FILL's
// 24-bit fill color rides in the header's low 24 bits — and prm_[1..n] hold
// the parameter words counted by gp0_param_words().
//
// This exercise builds the packet layer and GP0(02h) FILL. Rendering
// commands are decoded (correct parameter lengths) but produce no pixels
// yet — the rasterizer arrives in exercise 03.
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

//@LABS-BEGIN 1
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(1): return the parameter-word count for every GP0 opcode family per
// PSX-SPX (see the counts hinted in the tests); keep unknown opcodes at 0.
inline constexpr uint32_t gp0_param_words(uint8_t) {
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
void Gpu::reset() {
    // Power-on GPUSTAT reads 14802000h: FIFO empty (bit28), ready for
    // commands (bit26), display disabled (bit23), interlace field high
    // (bit13). Reset does NOT clear VRAM.
    head_ = tail_ = count_ = 0;
    mode_ = Mode::Idle;
    cmd_ = params_left_ = prm_n_ = 0;
    xfer_left_ = xf_pos_ = xf_w_ = 0;
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
//@LABS-STUB
// TODO(2): implement reset() (FIFO/parser/read-FIFO cleared, regs to
// power-on defaults, VRAM untouched), the bounded ring push in write_gp0()
// (false when full) and drain() (feed every queued word, oldest first).
void Gpu::reset() {
    regs = GpuRegs{};
}
bool Gpu::write_gp0(uint32_t) {
    return false;  // wrong on purpose
}
void Gpu::drain() {}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(3): dispatch on the opcode in bits 24-31 exactly as documented above.
// GP1(00h) restores power-on register state but must NOT clear VRAM.
void Gpu::write_gp1(uint32_t) {}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(4): implement feed() over the three parser modes and
// write_stream_pixel() with wrap + mask handling as documented above.
void Gpu::feed(uint32_t) {}
void Gpu::write_stream_pixel(uint16_t) {}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
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
        default: break;  // decoded rendering commands render in ex.03
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
//@LABS-STUB
// TODO(5): implement exec_current() dispatch (FILL via fill_vram using the
// header color, A0h streaming setup, C0h latching incl. odd-halfword
// padding, E1..E6 attribute storage, IRQ1). Rendering commands stay no-ops.
void Gpu::exec_current() {}
void Gpu::apply_copy_with_mask(std::span<const uint16_t>, int, int, uint32_t,
                               uint32_t) {}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(6): assemble successive little-endian words from read_fifo_,
// returning 0 once exhausted.
uint32_t Gpu::gpuread() {
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 7
//@LABS-SOLUTION
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
//@LABS-STUB
// TODO(7): compose GPUSTAT exactly per the PSX-SPX table above; power-on
// must read 14802000h.
uint32_t Gpu::status() const {
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace psx::gpu
