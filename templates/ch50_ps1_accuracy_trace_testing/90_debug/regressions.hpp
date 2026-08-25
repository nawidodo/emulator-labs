#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

// Ten seeded regressions for the debugging exercise.
//
// Each numbered block below carries ONE historical accuracy bug of the kind
// real emulator development actually ships: a sign-extension slip, a trace
// that reports post-increment pcs, an off-by-one fill, a stride ignored, a
// double-applied envelope quantum, DMA counts and chain flags, a GTE shift
// applied at the wrong pipeline stage, a target compare that misses exact
// matches, and a BCD decode with swapped nibbles.
//
// The STUB side of every block is the SEEDED BUG; the SOLUTION side is the
// corrected behaviour. main.cpp pins each bug's invariant with exactly one
// TEST (suite seed01..seed10), so `ctest` on a skeleton fails ten specific
// cases and goes green as each is diagnosed. Symptoms are documented,
// numbered to match, in DEBUGGING.md.
namespace psxmini::regress {

// Shared trace formatter — identical contract to psx_mini.hpp's fmt_trace.
inline std::string fmt_trace(uint32_t pc, uint32_t op_word, uint64_t cyc) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "pc=%08X op=%08X cyc=%llu", pc, op_word,
                  static_cast<unsigned long long>(cyc));
    return buf;
}

// --- state types used by the kernels below (identical on both sides) ---

struct StepCpu {
    uint32_t pc = 0;   // byte address of the next instruction to execute
    uint64_t cyc = 0;  // cumulative cycles
};

struct RVram {
    static constexpr unsigned kW = 64, kH = 32;
    std::array<uint16_t, kW * kH> p{};
};

struct RDma {
    uint32_t madr = 0;       // RAM byte address
    uint32_t words_left = 0;
    bool enable = true;
    bool irq = false;
};

struct RDesc {
    uint32_t madr;
    uint32_t words;
    uint16_t next;  // index of the next descriptor; 0xFFFF ends the chain
};

struct RTimer {
    uint16_t cnt = 0;
    uint16_t target = 37;
    unsigned reached = 0;
};

//@LABS-BEGIN 1
// Seed 01 — ALU immediate decode. Streams that add negative immediates
// (loop countdowns!) walk off into huge positive values.
//@LABS-SOLUTION
inline uint32_t exec_addi(uint32_t rs_val, uint16_t imm12) {
    const int32_t simm =
        static_cast<int16_t>(static_cast<uint16_t>(imm12 << 4)) >> 4;
    return static_cast<uint32_t>(static_cast<int32_t>(rs_val) + simm);
}
//@LABS-STUB
inline uint32_t exec_addi(uint32_t rs_val, uint16_t imm12) {
    // TODO(1): seeded bug — see DEBUGGING.md seed 01. Do not change anything
    // beyond the defect you diagnose.
    return rs_val + imm12;
}
//@LABS-END

//@LABS-BEGIN 2
// Seed 02 — golden traces report the pc the instruction STARTED at.
// Trace comparisons against committed goldens diverge on EVERY line while
// the machine still behaves identically — the nastiest kind of suite failure.
//@LABS-SOLUTION
inline std::string step_trace(StepCpu& c, uint32_t op_word) {
    const uint32_t at = c.pc;
    c.pc += 4;
    c.cyc += 1;
    return fmt_trace(at, op_word, c.cyc);
}
//@LABS-STUB
inline std::string step_trace(StepCpu& c, uint32_t op_word) {
    // TODO(2): seeded bug — see DEBUGGING.md seed 02.
    c.pc += 4;
    c.cyc += 1;
    return fmt_trace(c.pc, op_word, c.cyc);
}
//@LABS-END

//@LABS-BEGIN 3
// Seed 03 — rectangle fills lose their last column; sprites gain a
// one-pixel transparent seam on the right edge.
//@LABS-SOLUTION
inline void gpu_fill(RVram& v, unsigned x, unsigned y, unsigned w, unsigned h,
                     uint16_t col) {
    for (unsigned j = 0; j < h; ++j)
        for (unsigned i = 0; i < w; ++i)
            if (x + i < RVram::kW && y + j < RVram::kH)
                v.p[(y + j) * RVram::kW + x + i] = col;
}
//@LABS-STUB
inline void gpu_fill(RVram& v, unsigned x, unsigned y, unsigned w, unsigned h,
                     uint16_t col) {
    // TODO(3): seeded bug — see DEBUGGING.md seed 03.
    for (unsigned j = 0; j < h; ++j)
        for (unsigned i = 0; i + 1 < w; ++i)
            if (x + i < RVram::kW && y + j < RVram::kH)
                v.p[(y + j) * RVram::kW + x + i] = col;
}
//@LABS-END

//@LABS-BEGIN 4
// Seed 04 — CPU->VRAM blits honour the SOURCE stride, not the destination
// row width. Any texture narrower than its buffer arrives scrambled.
//@LABS-SOLUTION
inline void blit_rows(RVram& v, const uint16_t* src, unsigned src_stride,
                      unsigned x, unsigned y, unsigned rows, unsigned cols) {
    for (unsigned j = 0; j < rows; ++j)
        for (unsigned i = 0; i < cols; ++i)
            if (x + i < RVram::kW && y + j < RVram::kH)
                v.p[(y + j) * RVram::kW + x + i] = src[j * src_stride + i];
}
//@LABS-STUB
inline void blit_rows(RVram& v, const uint16_t* src, unsigned src_stride,
                      unsigned x, unsigned y, unsigned rows, unsigned cols) {
    // TODO(4): seeded bug — see DEBUGGING.md seed 04.
    for (unsigned j = 0; j < rows; ++j)
        for (unsigned i = 0; i < cols; ++i)
            if (x + i < RVram::kW && y + j < RVram::kH)
                v.p[(y + j) * RVram::kW + x + i] = src[j * cols + i];
}
//@LABS-END

//@LABS-BEGIN 5
// Seed 05 — exponential envelope quanta. A release that falls twice per
// sample halves the audible tail length of every sound.
//@LABS-SOLUTION
inline int32_t env_exp_release(int32_t level, unsigned rate_code) {
    int32_t delta = (level * static_cast<int32_t>(rate_code)) >> 6;
    if (delta < 1) delta = 1;
    level -= delta;
    return level < 0 ? 0 : level;  // envelopes clamp at silence
}
//@LABS-STUB
inline int32_t env_exp_release(int32_t level, unsigned rate_code) {
    // TODO(5): seeded bug — see DEBUGGING.md seed 05.
    int32_t delta = (level * static_cast<int32_t>(rate_code)) >> 6;
    if (delta < 1) delta = 1;
    return level - delta - delta;
}
//@LABS-END

//@LABS-BEGIN 6
// Seed 06 — DMA block transfers move one word too many, stomping whatever
// sits behind the device buffer.
//@LABS-SOLUTION
inline uint32_t dma_run_block(RDma& ch, const uint32_t* ram, size_t ram_words,
                              uint32_t* dev) {
    uint32_t copied = 0;
    while (ch.words_left > 0) {
        dev[copied++] = ram[(ch.madr >> 2) % ram_words];
        ch.madr += 4;
        ch.words_left--;
    }
    ch.enable = false;
    ch.irq = true;
    return copied;
}
//@LABS-STUB
inline uint32_t dma_run_block(RDma& ch, const uint32_t* ram, size_t ram_words,
                              uint32_t* dev) {
    // TODO(6): seeded bug — see DEBUGGING.md seed 06.
    uint32_t copied = 0;
    while (ch.words_left > 0) {
        dev[copied++] = ram[(ch.madr >> 2) % ram_words];
        ch.madr += 4;
        ch.words_left--;
    }
    dev[copied++] = ram[(ch.madr >> 2) % ram_words];
    ch.enable = false;
    ch.irq = true;
    return copied;
}
//@LABS-END

//@LABS-BEGIN 7
// Seed 07 — descriptor chains finish but leave the channel enabled, so the
// next poll re-triggers the whole chain from descriptor zero.
//@LABS-SOLUTION
inline void dma_run_chain(RDma& ch, const RDesc* table, size_t table_len,
                          const uint32_t* ram, size_t ram_words,
                          uint32_t* dev, uint32_t dev_cap) {
    uint32_t out = 0;
    uint16_t idx = 0;
    while (idx != 0xFFFF && idx < table_len) {
        const RDesc& d = table[idx];
        uint32_t addr = d.madr;
        for (uint32_t k = 0; k < d.words && out < dev_cap; ++k) {
            dev[out++] = ram[(addr >> 2) % ram_words];
            addr += 4;
        }
        idx = d.next;
    }
    ch.words_left = 0;
    ch.enable = false;  // hardware clears enable when the chain retires
    ch.irq = true;
}
//@LABS-STUB
inline void dma_run_chain(RDma& ch, const RDesc* table, size_t table_len,
                          const uint32_t* ram, size_t ram_words,
                          uint32_t* dev, uint32_t dev_cap) {
    // TODO(7): seeded bug — see DEBUGGING.md seed 07.
    uint32_t out = 0;
    uint16_t idx = 0;
    while (idx != 0xFFFF && idx < table_len) {
        const RDesc& d = table[idx];
        uint32_t addr = d.madr;
        for (uint32_t k = 0; k < d.words && out < dev_cap; ++k) {
            dev[out++] = ram[(addr >> 2) % ram_words];
            addr += 4;
        }
        idx = d.next;
    }
    ch.words_left = 0;
    ch.enable = true;
    ch.irq = true;
}
//@LABS-END

//@LABS-BEGIN 8
// Seed 08 — the GTE >>12 must land AFTER the dot product sums. Shifting
// each term separately throws away carry bits and every projected vertex
// drifts by whole pixels.
//@LABS-SOLUTION
inline int32_t gte_mac_y(int16_t m0, int16_t m1, int16_t vx, int16_t vy,
                         int16_t ty) {
    const int32_t dot = static_cast<int32_t>(m0) * vx +
                        static_cast<int32_t>(m1) * vy;
    return (dot >> 12) + ty;
}
//@LABS-STUB
inline int32_t gte_mac_y(int16_t m0, int16_t m1, int16_t vx, int16_t vy,
                         int16_t ty) {
    // TODO(8): seeded bug — see DEBUGGING.md seed 08.
    const int32_t p0 = (static_cast<int32_t>(m0) * vx) >> 12;
    const int32_t p1 = (static_cast<int32_t>(m1) * vy) >> 12;
    return p0 + p1 + ty;
}
//@LABS-END

//@LABS-BEGIN 9
// Seed 09 — timer targets fire on EXACT equality. An overshoot compare
// never fires when increments land exactly on the target, so periodic
// interrupts drift or vanish outright.
//@LABS-SOLUTION
inline void timer_tick(RTimer& t) {
    if (t.cnt == t.target) {
        t.cnt = 0;
        ++t.reached;
    } else {
        ++t.cnt;
    }
}
//@LABS-STUB
inline void timer_tick(RTimer& t) {
    // TODO(9): seeded bug — see DEBUGGING.md seed 09.
    if (t.cnt > t.target) {
        t.cnt = 0;
        ++t.reached;
    } else {
        ++t.cnt;
    }
}
//@LABS-END

//@LABS-BEGIN 10
// Seed 10 — CDROM MSF fields arrive BCD-coded; decoding them nibble-swapped
// seeks to the wrong minute every time both digits differ.
//@LABS-SOLUTION
inline unsigned cdrom_bcd_to_dec(uint8_t v) {
    return (v >> 4) * 10u + (v & 0x0Fu);
}
//@LABS-STUB
inline unsigned cdrom_bcd_to_dec(uint8_t v) {
    // TODO(10): seeded bug — see DEBUGGING.md seed 10.
    return (v & 0x0Fu) * 10u + (v >> 4);
}
//@LABS-END

}  // namespace psxmini::regress
