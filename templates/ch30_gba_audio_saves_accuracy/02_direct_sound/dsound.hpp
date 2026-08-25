#pragma once
// GBA Direct Sound: FIFO A/B, timer-clocked sampling, volume shift and
// SOUNDBIAS quantization.
#include <cstdint>
#include <vector>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr int kFifoSize = 32;

#ifndef GBA_FNV64_DEFINED
#define GBA_FNV64_DEFINED
inline u64 fnv64(const void* data, size_t n) {
    u64 h = 0xCBF29CE484222325ull;
    const u8* p = static_cast<const u8*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}
#endif

struct SoundFifo {
    u8 data[kFifoSize] = {};
    int rd = 0;    // index of oldest byte
    int used = 0;

    void reset() {
        rd = 0;
        used = 0;
        for (auto& b : data) b = 0x80;  // silence pattern (unsigned mid)
    }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    void push(u8 b) {
        data[(rd + used) % kFifoSize] = b;
        if (used < kFifoSize)
            ++used;
        else
            rd = (rd + 1) % kFifoSize;  // overwrite oldest, like hardware
    }

    // Timer overflow pops one byte. An EMPTY FIFO holds the last sample —
    // hardware keeps outputting the stale byte until refilled.
    s8 pop() {
        if (used == 0) return s8(data[(rd + kFifoSize - 1) % kFifoSize]);
        s8 v = s8(data[rd]);
        rd = (rd + 1) % kFifoSize;
        --used;
        return v;
    }
//@LABS-STUB
    // TODO(1): implement push (ring buffer, overwrite oldest when full) and
    // pop (returns the OLDEST byte as signed; an empty FIFO repeats the
    // last delivered byte instead of advancing).
    void push(u8) {}
    s8 pop() { return 0; }  // wrong on purpose
//@LABS-END
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
// SOUNDCNT_H per-channel volume field (bits 0-1 for A, 8-9 for B):
// 0 -> 25%, 1 -> 50%, 2 -> 100%, 3 prohibited -> treated as mute.
inline s8 apply_dsound_volume(s8 sample, int vol_code) {
    switch (vol_code & 3) {
        case 0: return s8(sample >> 2);
        case 1: return s8(sample >> 1);
        case 2: return sample;
        default: return 0;
    }
}
//@LABS-STUB
// TODO(2): scale the signed sample by the volume code: 0 -> >>2 (25%),
// 1 -> >>1 (50%), 2 -> unchanged (100%), 3 -> muted.
inline s8 apply_dsound_volume(s8 sample, int vol_code) {
    (void)sample;
    (void)vol_code;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// SOUNDBIAS: add the 10-bit bias level to a mixed sample, then quantize to
// the resolution grid (resolution bits 14-15 select how many low bits of
// the 10-bit result are dropped). Returns the final unsigned 10-bit value.
inline u16 bias_quantize(int mixed, u16 bias_reg) {
    int bias = bias_reg & 0x3FF;
    int res = (bias_reg >> 14) & 3;
    int v = mixed + bias;
    if (v < 0) v = 0;
    if (v > 1023) v = 1023;
    if (res) v &= ~((1 << res) - 1);
    return u16(v);
}
//@LABS-STUB
// TODO(3): clamp (mixed + 10-bit bias) into [0,1023], then zero the low
// `resolution` bits selected by bits 14-15 of the bias register.
inline u16 bias_quantize(int mixed, u16 bias_reg) {
    (void)mixed;
    (void)bias_reg;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Render `samples` PCM frames: every `timer_period` guest cycles each FIFO
// pops one byte, scaled by its volume code and summed with PSG levels,
// then bias-quantized into an unsigned 10-bit value written as little-
// endian u16. Deterministic; returns the FNV-64 of the produced buffer.
inline u64 render_pcm(SoundFifo& fifo_a, SoundFifo& fifo_b, int psg_level,
                      int vol_a, int vol_b, u16 bias_reg, u64 timer_period,
                      int samples, std::vector<u16>& out) {
    (void)timer_period;  // fixed model: one frame per pop
    out.clear();
    out.reserve(size_t(samples));
    for (int i = 0; i < samples; ++i) {
        int mixed = psg_level + apply_dsound_volume(fifo_a.pop(), vol_a) +
                    apply_dsound_volume(fifo_b.pop(), vol_b) * 2;
        out.push_back(bias_quantize(mixed, bias_reg));
    }
    return fnv64(out.data(), out.size() * sizeof(u16));
}
//@LABS-STUB
// TODO(4): loop `samples` times advancing one timer period per frame: pop
// both FIFOs, scale by volume codes (B counts double in this model),
// add the PSG level, bias-quantize, append to `out`; return FNV-64 of the
// raw u16 buffer.
inline u64 render_pcm(SoundFifo&, SoundFifo&, int, int, int, u16, u64,
                      int, std::vector<u16>& out) {
    out.clear();
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace gba
