#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "apu_channels.hpp"

// Chapter 24 — the synchronized machine: frame counter, OAM DMA cycle
// accounting, and the CPU:PPU catch-up scheduler.
//
// Course model (documented, deterministic, integer-only):
//
//   * Every CPU cycle advances the whole machine: the PPU catches up with
//     exactly kPpuDotsPerCpu (hardware: 3) dots per CPU cycle, stepping
//     devices PER DOT; the APU ticks once per CPU cycle.
//   * OAM DMA ($4014) costs 513 CPU cycles starting on an even cycle,
//     514 on an odd one (extra alignment cycle). The CPU is stalled but
//     PPU/APU keep running — how a sprite DMA shifts a raster split.
//   * Frame counter (course model): 4-step quarters at CPU cycles 7457,
//     14913, 22371, 29829 (halves at 14913/29829); IRQ at 29829 unless
//     inhibited; wrap at 29830. 5-step wraps at 37282 with halves at
//     14913/37281 and never asserts IRQ. $4017 takes effect immediately
//     (documented simplification).
//   * DMC fetches beyond the level trigger are a documented stub:
//     needs_fetch() flags, no cycles stolen.
//   * Rendering is a course-model scanline renderer (background only):
//     each visible scanline re-reads the scroll latches, so mid-frame
//     $2005/$2006-style splits still land row-exact. Sprites are out of
//     scope for hashing purposes.
namespace nes24sync {

constexpr int kPpuDotsPerCpu = 3;   // NTSC master clock ratio 12:4

constexpr int kCpuCyclesPerOp = 2;  // scripted ops cost this many cycles

struct FrameCounter {
    uint64_t cycle = 0;
    bool five_step = false, inhibit = false;
    int quarters = 0, halves = 0;
    bool irq = false;

    void write(uint8_t v) {
        five_step = (v & 0x80) != 0;
        inhibit = (v & 0x40) != 0;
        if (inhibit) irq = false;
        cycle = 0;
    }

    void tick() {  // one APU tick == one CPU cycle here
        ++cycle;
        if (!five_step) {
            if (cycle == 7457 || cycle == 14913 || cycle == 22371 ||
                cycle == 29829)
                ++quarters;
            if (cycle == 14913 || cycle == 29829) ++halves;
            if (cycle == 29829 && !inhibit) irq = true;
            if (cycle >= 29830) cycle = 0;
        } else {
            if (cycle == 7457 || cycle == 14913 || cycle == 29829)
                ++quarters;
            if (cycle == 14913 || cycle == 37281) ++halves;
            if (cycle >= 37282) cycle = 0;
        }
    }
};

struct ApuLite {
    nes24apu::Pulse pulse1{0}, pulse2{1};
    nes24apu::Triangle triangle;
    nes24apu::Noise noise;
    nes24apu::Dmc dmc;
    FrameCounter frame;

    void write_reg(uint16_t addr, uint8_t v) {
        if (addr <= 0x4003) pulse1.write_reg(addr & 3, v);
        else if (addr <= 0x4007) pulse2.write_reg(addr & 3, v);
        else if (addr <= 0x400B) triangle.write_reg(addr & 3, v);
        else if (addr <= 0x400F) noise.write_reg(addr & 3, v);
        else if (addr <= 0x4013) dmc.write_reg(addr & 3, v);
        else if (addr == 0x4015) {
            pulse1.enabled = (v & 1) != 0; pulse2.enabled = (v & 2) != 0;
            triangle.enabled = (v & 4) != 0; noise.enabled = (v & 8) != 0;
            dmc.enabled = (v & 16) != 0;
            if (!(v & 16)) dmc.restart(0);
        } else if (addr == 0x4017) frame.write(v);
    }

    void tick_devices() {
        pulse1.tick_timer(); pulse2.tick_timer();
        triangle.tick_timer(); noise.tick_timer();
        frame.tick();
    }

    void quarter() {
        pulse1.tick_envelope(); pulse2.tick_envelope();
        triangle.tick_linear(); noise.tick_envelope();
    }

    void half() {
        pulse1.tick_length_and_sweep(); pulse2.tick_length_and_sweep();
        triangle.tick_length(); noise.tick_length();
    }

    int mix() const {
        return pulse1.output() + pulse2.output() + triangle.output() +
               noise.output() + dmc.output();
    }
};

struct MiniPpu {
    static constexpr int kW = 256, kH = 240;
    std::array<uint8_t, 0x800> vram{};
    std::array<uint8_t, 0x2000> chr{};
    std::array<uint8_t, 0x20> palette{};
    std::array<uint8_t, 0x100> oam{};
    uint8_t ctrl = 0, mask = 0;
    int scanline = 0, dot = 0;
    uint64_t frames_done = 0;
    int fine_x = 0, scroll_y = 0;
    std::vector<uint8_t> last_frame_rgba;

    // Horizontal arrangement: physical page = nametable bit 1 ($2000 vs
    // $2400 fold across $2800/$2C00).
    uint8_t vram_read(uint16_t addr) const {
        addr &= 0x0FFF;
        uint16_t page = (addr >> 11) & 1;
        return vram[((addr & 0x03FF) | (page << 10))];
    }

    void render_scanline(int sy) {
        last_frame_rgba.resize(size_t(kW) * kH * 4);
        uint16_t nt_base = (ctrl & 1) ? 0x400 : 0;
        for (int x = 0; x < kW; ++x) {
            uint8_t px = palette[0];
            if (mask & 0x08) {
                int fx = fine_x + x;
                int ty = ((scroll_y + sy) >> 3) % 30;
                int row = (scroll_y + sy) & 7;
                int tx = fx >> 3;
                uint16_t nt = nt_base + uint16_t((ty * 32 + tx) & 0x3FF);
                uint8_t tile = vram_read(nt);
                uint16_t at = nt_base + 0x3C0 +
                              uint16_t(((ty >> 2) * 8) + (tx >> 2));
                uint8_t attr = vram_read(at);
                int shift = ((ty & 2) << 1) | (tx & 2);
                int pal = (attr >> shift) & 3;
                uint16_t pat = uint16_t(tile * 16 + row);
                uint8_t lo = (chr[pat] >> (7 - (fx & 7))) & 1;
                uint8_t hi = (chr[(pat + 8) & 0x1FFF] >> (7 - (fx & 7))) & 1;
                int color = lo | (hi << 1);
                if (color != 0) px = palette[pal * 4 + color];
            }
            uint8_t* out =
                &last_frame_rgba[(size_t(sy) * kW + size_t(x)) * 4];
            out[0] = out[1] = out[2] = px;
            out[3] = 255;
        }
    }

    void tick(ApuLite& /*apu*/) {
        if (scanline < 240 && dot == 0) render_scanline(scanline);
        if (++dot >= 341) {
            dot = 0;
            if (++scanline >= 262) {
                scanline = 0;
                ++frames_done;
            }
        }
    }
};

struct Machine {
    ApuLite apu;
    MiniPpu ppu;
    std::array<uint8_t, 0x800> ram{};
    uint64_t cpu_cycle = 0;

    // OAM DMA state
    bool dma_pending = false, dma_active = false;
    uint8_t dma_page = 0;
    int dma_elapsed = 0, dma_dummy = 0;

    std::vector<int16_t> audio;
    int w_toggle_ = 0;

    // One full CPU cycle. Order matters and is part of the contract:
    // begin/continue DMA -> PPU catches up -> APU ticks -> clocks dispatch
    // -> one audio sample lands.
    void cpu_tick() {
        if (!dma_active && dma_pending) {
            dma_pending = false;
            dma_active = true;
            dma_elapsed = 0;
            dma_dummy = (cpu_cycle & 1) ? 2 : 1;  // alignment rule
        }
        if (dma_active) {
            ++dma_elapsed;
            if (dma_elapsed > dma_dummy &&
                ((dma_elapsed - dma_dummy) & 1) == 0) {
                int i = (dma_elapsed - dma_dummy) / 2 - 1;
                if (i >= 0 && i < 256)
                    ppu.oam[size_t(i)] =
                        ram[(uint16_t(dma_page) << 8) | uint8_t(i)];
            }
            if (dma_elapsed >= dma_dummy + 512) dma_active = false;
        }
        int q0 = apu.frame.quarters, h0 = apu.frame.halves;
        for (int i = 0; i < kPpuDotsPerCpu; ++i) ppu.tick(apu);
        apu.tick_devices();
        audio.push_back(int16_t(apu.mix() * 512));
        if (apu.frame.quarters != q0) apu.quarter();
        if (apu.frame.halves != h0) apu.half();
        ++cpu_cycle;
    }

    void request_oam_dma(uint8_t page) { dma_page = page; dma_pending = true; }

    // Scripted CPU-side store (register decode included), then pay for it.
    void cpu_write(uint16_t addr, uint8_t v) {
        if (addr < 0x2000) {
            ram[addr & 0x7FF] = v;
        } else if (addr < 0x4000) {
            switch (addr & 7) {
                case 0: ppu.ctrl = v; break;
                case 1: ppu.mask = v; break;
                case 5:
                    w_toggle_ ^= 1;
                    if (w_toggle_) ppu.fine_x = v & 7;
                    else ppu.scroll_y = v;
                    break;
                default: break;  // other PPU ports unused by this model
            }
        } else if (addr == 0x4014) {
            request_oam_dma(v);
        } else if (addr == 0x4015 || addr == 0x4017 ||
                   (addr >= 0x4000 && addr <= 0x4013)) {
            apu.write_reg(addr, v);
        }
        for (int i = 0; i < kCpuCyclesPerOp; ++i) cpu_tick();
    }

    void run_one_frame() {
        uint64_t f0 = ppu.frames_done;
        while (ppu.frames_done == f0) cpu_tick();
    }

    void run_frames(int n) {
        for (int i = 0; i < n; ++i) run_one_frame();
    }
};

}  // namespace nes24sync
