#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "fnv.hpp"

// psx-mini — the chapter's compact reference machine.
//
// This is NOT a real PS1 core. It is a deliberately small, fully
// deterministic model of the seven subsystems this chapter pins with
// accuracy suites (see ../SPEC.md for the full register-level contract):
//
//   MiniCpu   : 8-register word machine, 1/2-cycle instructions, emits
//               `pc= op= cyc=` trace lines in the course-wide format.
//   MiniGpu   : 64x32 15-bit VRAM with fill / checker / gradient draws.
//   MiniSpu   : one wavetable voice with a linear decay envelope.
//   DmaChan   : block-transfer channel (madr/wc/step/enable/irq flags).
//   MiniGte   : 3x3 fixed-point vector/matrix MAC pipeline (>>12 shift).
//   MiniTimer : prescaled 16-bit counter with exact target reload.
//   MiniCdrom : MSF/BCD sector addressing over a synthetic image.
//
// Every operation is pure integer math: no clocks, no RNG, no host state.
// Golden hashes in shared/goldens.hpp were produced by THIS code and are
// reproduced by the built-in checks in 01_suite_runner/checks.hpp.
namespace psxmini {

// ---------------------------------------------------------------------------
// MiniCpu
// ---------------------------------------------------------------------------

inline constexpr unsigned kOpHalt = 0xFF, kOpAddi = 0x01, kOpAdd = 0x02,
    kOpSub = 0x03, kOpXor = 0x04, kOpSll = 0x05, kOpSra = 0x06, kOpLw = 0x07,
    kOpSw = 0x08, kOpLi = 0x09, kOpBeq = 0x0A, kOpBne = 0x0B;

constexpr uint32_t enc(unsigned op, unsigned rd, unsigned rs, unsigned rt,
                       unsigned imm) {
    return (op << 24) | (rd << 20) | (rs << 16) | (rt << 12) | (imm & 0xFFF);
}

struct Ram {
    static constexpr uint32_t kBytes = 4096;
    std::array<uint32_t, kBytes / 4> w{};

    uint32_t load(uint32_t byte_addr) const {
        return w[(byte_addr >> 2) & (w.size() - 1)];
    }
    void store(uint32_t byte_addr, uint32_t v) {
        w[(byte_addr >> 2) & (w.size() - 1)] = v;
    }
};

struct Cpu {
    uint32_t r[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t pc = 0;      // byte address of the NEXT instruction to execute
    uint64_t cyc = 0;     // cumulative guest cycles
    bool halted = false;
};

// One trace line in the course-wide format (docs/AUTHORING.md):
//   pc=<8 hex> op=<8 hex> cyc=<decimal>
inline std::string fmt_trace(uint32_t pc, uint32_t op_word, uint64_t cyc) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "pc=%08X op=%08X cyc=%llu", pc, op_word,
                  static_cast<unsigned long long>(cyc));
    return buf;
}

// Execute ONE instruction. The trace line reports the pc the instruction
// STARTED at and the cumulative cycle count INCLUDING this instruction.
// Cycle costs: ALU/LI/untaken-branch/HALT = 1, LW/SW = 2, taken branch = 2.
inline bool cpu_step(Cpu& c, Ram& m, std::string* trace) {
    const uint32_t at = c.pc;
    const uint32_t w = m.load(at);
    c.pc += 4;

    const unsigned op = w >> 24;
    const unsigned rd = (w >> 20) & 0xF;
    const unsigned rs = (w >> 16) & 0xF;
    const unsigned rt = (w >> 12) & 0xF;
    const uint32_t imm = w & 0xFFF;
    const int32_t simm = static_cast<int16_t>(static_cast<uint16_t>(imm << 4)) >> 4;
    unsigned cost = 1;

    switch (op) {
        case kOpHalt: c.halted = true; break;
        case kOpLi:   c.r[rd] = imm; break;
        case kOpAddi: c.r[rd] = static_cast<uint32_t>(
                          static_cast<int32_t>(c.r[rs]) + simm); break;
        case kOpAdd:  c.r[rd] = c.r[rs] + c.r[rt]; break;
        case kOpSub:  c.r[rd] = c.r[rs] - c.r[rt]; break;
        case kOpXor:  c.r[rd] = c.r[rs] ^ c.r[rt]; break;
        case kOpSll:  c.r[rd] = c.r[rt] << (imm & 31); break;
        case kOpSra:  c.r[rd] = static_cast<uint32_t>(
                          static_cast<int32_t>(c.r[rt]) >> (imm & 31)); break;
        case kOpLw:   cost = 2; c.r[rd] = m.load(c.r[rs] + imm); break;
        case kOpSw:   cost = 2; m.store(c.r[rs] + imm, c.r[rt]); break;
        case kOpBeq:
        case kOpBne: {
            const bool cond =
                (op == kOpBeq) ? (c.r[rs] == c.r[rt]) : (c.r[rs] != c.r[rt]);
            if (cond) {          // target relative to the delay slot (at+4)
                cost = 2;
                c.pc = at + 4 + static_cast<uint32_t>(simm * 4);
            }
            break;
        }
        default: c.halted = true; break;  // illegal opcode halts
    }

    c.cyc += cost;
    if (trace) trace->append(fmt_trace(at, w, c.cyc)).append(1, '\n');
    return !c.halted;
}

inline std::string run_trace(Cpu& c, Ram& m, unsigned max_steps) {
    std::string out;
    for (unsigned i = 0; i < max_steps && !c.halted; ++i)
        cpu_step(c, m, &out);
    return out;
}

// ---------------------------------------------------------------------------
// MiniGpu — 64 x 32 x 15-bit VRAM
// ---------------------------------------------------------------------------

struct Vram {
    static constexpr unsigned kW = 64, kH = 32;
    std::array<uint16_t, kW * kH> p{};
};

inline void gpu_fill(Vram& v, unsigned x, unsigned y, unsigned w, unsigned h,
                     uint16_t c) {
    for (unsigned j = 0; j < h; ++j)
        for (unsigned i = 0; i < w; ++i)
            if (x + i < Vram::kW && y + j < Vram::kH)
                v.p[(y + j) * Vram::kW + x + i] = c;
}

inline void gpu_checker(Vram& v, unsigned x, unsigned y, unsigned w,
                        unsigned h, uint16_t a, uint16_t b) {
    for (unsigned j = 0; j < h; ++j)
        for (unsigned i = 0; i < w; ++i)
            if (x + i < Vram::kW && y + j < Vram::kH)
                v.p[(y + j) * Vram::kW + x + i] = ((x + i) ^ (y + j)) & 1 ? b : a;
}

inline void gpu_hgradient(Vram& v, unsigned y, uint16_t c0, uint16_t step) {
    for (unsigned i = 0; i < Vram::kW; ++i)
        v.p[y * Vram::kW + i] = static_cast<uint16_t>(c0 + i * step);
}

inline uint64_t hash_vram(const Vram& v) {
    auto* b = reinterpret_cast<const uint8_t*>(v.p.data());
    return fnv64({b, v.p.size() * sizeof(uint16_t)});
}

// ---------------------------------------------------------------------------
// MiniSpu — one wavetable voice, mono
// ---------------------------------------------------------------------------

// Symmetric triangle wave in [-105, 105]: keeps every product inside int32
// while still exercising negative samples.
constexpr std::array<int8_t, 16> kWaveTable = [] {
    std::array<int8_t, 16> t{};
    for (int i = 0; i < 16; ++i)
        t[i] = static_cast<int8_t>((i < 8 ? i : 15 - i) * 30 - 105);
    return t;
}();

struct SpuVoice {
    uint32_t phase = 0;       // 8.24 fixed point; table index = phase >> 28
    uint32_t pitch = 0x1000;  // 4096 == one full table sweep per 16 samples
    int32_t env = 4096;       // linear envelope, decays every sample
};

inline int16_t spu_next(SpuVoice& v) {
    const unsigned idx = (v.phase >> 28) & 0xF;
    int32_t s = (static_cast<int32_t>(kWaveTable[idx]) * v.env) >> 12;
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    v.phase += v.pitch << 16;
    v.env -= 24;              // linear decay, clamps at zero
    if (v.env < 0) v.env = 0;
    return static_cast<int16_t>(s);
}

inline uint64_t render_spu_hash(SpuVoice v, unsigned n) {
    uint64_t h = kFnvOffset;
    // Fold samples straight into FNV-1a (big-endian byte order per sample)
    // so no intermediate buffer allocation is needed.
    for (unsigned i = 0; i < n; ++i) {
        const auto s = static_cast<uint16_t>(spu_next(v));
        for (int b = 1; b >= 0; --b) {
            h ^= static_cast<uint8_t>(s >> (8 * b));
            h *= kFnvPrime;
        }
    }
    return h;
}

// ---------------------------------------------------------------------------
// DmaChan — one block-transfer channel
// ---------------------------------------------------------------------------

struct DmaChan {
    uint32_t madr = 0;      // RAM byte address
    uint32_t bcr = 0;       // (words << 16) | unused — words in high half
    uint32_t step = 4;      // madr advance per word
    bool enable = false;
    bool irq = false;
};
inline uint32_t dma_words(const DmaChan& ch) { return ch.bcr >> 16; }

// Copies `dma_words` words from `ram` (1024 words) into `dev`, then clears
// enable and latches irq — the observable end-of-block state a suite pins.
template <size_t N>
inline void dma_run_block(DmaChan& ch, const std::array<uint32_t, N>& ram,
                          uint32_t* dev) {
    const uint32_t n = dma_words(ch);
    const uint32_t base = ch.madr >> 2;
    for (uint32_t k = 0; k < n; ++k) dev[k] = ram[(base + k) & (N - 1)];
    ch.madr += ch.step * n;
    ch.bcr = 0;
    ch.enable = false;
    ch.irq = true;
}

// ---------------------------------------------------------------------------
// MiniGte — fixed-point MAC pipeline
// ---------------------------------------------------------------------------

struct MiniGte {
    int16_t m[3][3]{};  // row-major
    int16_t tr[3]{};

    // mac_i = sum_k m[i][k]*v[k] + tr[i]; ir_i = sat16(mac_i >> 12).
    // The >>12 happens ONCE on the accumulated dot product — shifting each
    // term separately loses carry bits and diverges from the pinned values.
    void mul_vector(const int16_t v[3], int32_t mac[3], int16_t ir[3]) const {
        for (int i = 0; i < 3; ++i) {
            mac[i] = static_cast<int32_t>(m[i][0]) * v[0] +
                     static_cast<int32_t>(m[i][1]) * v[1] +
                     static_cast<int32_t>(m[i][2]) * v[2] + tr[i];
            int32_t shifted = mac[i] >> 12;
            if (shifted > 32767) shifted = 32767;
            if (shifted < -32768) shifted = -32768;
            ir[i] = static_cast<int16_t>(shifted);
        }
    }
};

// ---------------------------------------------------------------------------
// MiniTimer — prescaled counter with exact-match target reload
// ---------------------------------------------------------------------------

struct MiniTimer {
    uint16_t cnt = 0;
    uint16_t target = 37;
    unsigned div = 4;      // sysclks per counter increment
    unsigned pres = 0;     // prescaler phase
    unsigned reached = 0;  // times the target has been hit

    void tick_sysclk() {
        if (++pres < div) return;
        pres = 0;
        // Exact-match semantics like hardware: the reload fires on the tick
        // where cnt EQUALS target — never on overshoot.
        if (cnt == target) {
            cnt = 0;
            ++reached;
        } else {
            ++cnt;
        }
    }
};

// ---------------------------------------------------------------------------
// MiniCdrom — MSF addressing + BCD over a synthetic sector image
// ---------------------------------------------------------------------------

constexpr uint8_t bcd_enc(uint8_t v) {
    return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
}
constexpr uint8_t bcd_dec(uint8_t v) {
    return static_cast<uint8_t>(((v >> 4) * 10) + (v & 0x0F));
}

struct Msf {
    uint8_t m = 0, s = 0, f = 0;  // stored BCD-encoded, like the real header
};
inline Msf lba_to_msf(uint32_t lba) {
    return {bcd_enc(static_cast<uint8_t>(lba / 4500)),
            bcd_enc(static_cast<uint8_t>((lba / 75) % 60)),
            bcd_enc(static_cast<uint8_t>(lba % 75))};
}

// Sector payload is synthetic but stable: derived from an FNV stream seeded
// by the LBA, so any consumer can regenerate it without an image file.
inline void synth_sector(uint32_t lba, std::array<uint8_t, 32>& out) {
    uint64_t x = 0x9E3779B97F4A7C15ULL ^ lba;
    for (auto& b : out) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        b = static_cast<uint8_t>(x >> 32);
    }
    const Msf msf = lba_to_msf(lba);
    out[0] = msf.m;
    out[1] = msf.s;
    out[2] = msf.f;
    out[3] = 0x10;  // mode 1 marker
}

struct CdromState {
    uint32_t lba = 0;         // next sector to read
    unsigned sectors_read = 0;
    uint64_t data_hash = kFnvOffset;
};

inline void cdrom_read_sectors(CdromState& st, unsigned n) {
    for (unsigned i = 0; i < n; ++i, ++st.lba) {
        std::array<uint8_t, 32> sec{};
        synth_sector(st.lba, sec);
        st.data_hash = fnv64(std::span<const uint8_t>(sec)) ^ st.data_hash * kFnvPrime;
        ++st.sectors_read;
    }
}

}  // namespace psxmini
