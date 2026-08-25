#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "../03_pitch/pitch.hpp"
#include "../04_adsr/adsr.hpp"

namespace spu {

// ---------------------------------------------------------------------------
// The SPU proper: 24 voices behind a 0x200-byte register window at
// 1F801C00, 512 KiB of dedicated RAM fed by DMA, a CD-audio mixed input,
// an IRQ9 address comparator - and a reverb unit this course deliberately
// leaves OUT OF SCOPE (registers are accepted and stored, audio bypasses
// the effect).
//
// Register map implemented here (offsets from 1F801C00):
//   0x000 + v*0x10  voice v: VOL_L, VOL_R, PITCH, START_ADDR(>>3),
//                    ADSR1, ADSR2
//   0x180/0x182     main volume L/R          (unity at 0x4000, scale >>14)
//   0x188/0x18A     reverb output L/R        - stored, unused (bypass)
//   0x198/0x19A     CD audio input L/R
//   0x1B4           SPU IRQ address (in >>3 units)
//   0x1B6           SPU RAM DMA transfer address (>>3 units)
//   0x188..0x18F    (reverb config area)     - stored, ignored
//   0x1C0..0x1C6    KEY_ON bitmask (lo/mid/hi)
//   0x1CC..0x1D2    KEY_OFF bitmask
//   0x1D8           control (bit 15: SPU enable, bit 6: IRQ9 enable)
//   0x1DC           transfer control (DMA mode - accepted)
//   0x1E0..0x1FE    reverb register page     - stored, ignored (bypass)
//
// Volume rule used everywhere (documented simplification of the real
// saturating semantics): out = sample * vol >> 14, i.e. 0x4000 == x1.0;
// negative 16-bit values invert phase like the hardware's signed range.
//
// IRQ9 fires when any active voice's decode address or a DMA-written byte
// lands exactly on the configured address (and control.6 enables it).
// ---------------------------------------------------------------------------

class Spu {
public:
    static constexpr int kVoiceCount = 24;

    void reset();

    uint16_t read(uint32_t offset) const;
    void write(uint32_t offset, uint16_t value);

    // DMA: copy a chunk into SPU RAM at a byte address; returns bytes done.
    size_t dma_write(uint32_t byte_addr, std::span<const uint8_t> data);
    const SpuRam& ram() const { return ram_; }

    // CD-audio path stub: the real SPU mixes the CD decoder's stereo output
    // through the CD volume registers every frame.
    void set_cd_input(int16_t left, int16_t right);

    bool irq_flag() const { return irq_flag_; }
    void ack_irq() { irq_flag_ = false; }

    // Renders `frames` stereo frames, interleaved s16le, appended to out.
    void render(int frames, std::vector<int16_t>& out);

    const Adsr& envelope(int v) const { return adsr_[v]; }

private:
    void key_on_mask(uint64_t mask);
    void key_off_mask(uint64_t mask);

    SpuRam ram_{};
    std::array<VoiceRegs, kVoiceCount> regs_{};
    std::array<ResampledVoice, kVoiceCount> voices_{};
    std::array<Adsr, kVoiceCount> adsr_{};
    std::array<AdsrParams, kVoiceCount> adsr_params_{};

    uint16_t main_vol_l_ = 0, main_vol_r_ = 0;
    uint16_t cd_vol_l_ = 0, cd_vol_r_ = 0;
    uint16_t control_ = 0;
    uint16_t transfer_addr_ = 0;
    uint32_t reverb_work_area_ = 0;  // accepted, unused
    std::array<uint16_t, 16> reverb_regs_{};

    int16_t cd_in_l_ = 0, cd_in_r_ = 0;

    uint16_t irq_addr_word_ = 0;
    bool irq_flag_ = false;
};

//@LABS-BEGIN 10
//@LABS-SOLUTION
inline void Spu::reset() {
    ram_.data.fill(0);
    for (int v = 0; v < kVoiceCount; ++v) {
        regs_[v] = {};
        voices_[v].reset();
        adsr_[v] = {};
        adsr_params_[v] = {};
    }
    main_vol_l_ = main_vol_r_ = 0;
    cd_vol_l_ = cd_vol_r_ = 0;
    control_ = 0;
    transfer_addr_ = 0;
    reverb_work_area_ = 0;
    reverb_regs_.fill(0);
    cd_in_l_ = cd_in_r_ = 0;
    irq_addr_word_ = 0;
    irq_flag_ = false;
}

inline uint16_t Spu::read(uint32_t off) const {
    if (off < 0x180) {
        const int v = off >> 4;
        switch (off & 0xF) {
            case 0x0: return regs_[v].vol_left;
            case 0x2: return regs_[v].vol_right;
            case 0x4: return regs_[v].pitch;
            case 0x6: return regs_[v].start_addr;
            case 0x8: return regs_[v].adsr1;
            case 0xA: return regs_[v].adsr2;
        }
        return 0;
    }
    switch (off) {
        case 0x180: return main_vol_l_;
        case 0x182: return main_vol_r_;
        case 0x198: return cd_vol_l_;
        case 0x19A: return cd_vol_r_;
        case 0x1B4: return irq_addr_word_;
        case 0x1B6: return transfer_addr_;
        case 0x1D8: return control_;
        default: return 0;  // reverb pages read as stored zeroes
    }
}

inline void Spu::write(uint32_t off, uint16_t value) {
    if (off < 0x180) {
        const int v = off >> 4;
        VoiceRegs& r = regs_[v];
        switch (off & 0xF) {
            case 0x0: r.vol_left = value; break;
            case 0x2: r.vol_right = value; break;
            case 0x4: r.pitch = value; break;
            case 0x6: r.start_addr = value; break;
            case 0x8:
                r.adsr1 = value;
                adsr_params_[v] =
                    AdsrParams::unpack(r.adsr1, r.adsr2);
                break;
            case 0xA:
                r.adsr2 = value;
                adsr_params_[v] =
                    AdsrParams::unpack(r.adsr1, r.adsr2);
                break;
        }
        return;
    }
    switch (off) {
        case 0x180: main_vol_l_ = value; break;
        case 0x182: main_vol_r_ = value; break;
        case 0x198: cd_vol_l_ = value; break;
        case 0x19A: cd_vol_r_ = value; break;
        case 0x1B4: irq_addr_word_ = value; break;
        case 0x1B6: transfer_addr_ = value; break;
        case 0x1D8: control_ = value; break;
        default:
            // Reverb configuration: accepted and stored, audio bypasses it.
            if (off >= 0x1E0 && off < 0x200)
                reverb_regs_[(off - 0x1E0) >> 1] = value;
            else if (off >= 0x188 && off < 0x190)
                reverb_work_area_ = (reverb_work_area_ << 16) | value;
            break;
        case 0x1C0:
        case 0x1C1:
        case 0x1C2:
            key_on_mask(static_cast<uint64_t>(value)
                        << ((off - 0x1C0) * 16));
            break;
        case 0x1CC:
        case 0x1CD:
        case 0x1CE:
            key_off_mask(static_cast<uint64_t>(value)
                         << ((off - 0x1CC) * 16));
            break;
    }
}
//@LABS-STUB
// TODO(10): implement the register window.
//   read(): slice 0x000-0x17F by voice/field, plus the named globals in
//     the map comment. Unimplemented pages read 0.
//   write(): same slicing; writing ADSR1/ADSR2 also refreshes the cached
//     AdsrParams for that voice. Reverb ranges are STORED ONLY (bypass).
inline void Spu::reset() {
    // TODO(10): clear every register and voice back to defaults.
}

inline uint16_t Spu::read(uint32_t) const {
    // TODO(10): decode register reads.
    return 0;
}

inline void Spu::write(uint32_t, uint16_t) {
    // TODO(10): decode register writes.
}
//@LABS-END

//@LABS-BEGIN 11
//@LABS-SOLUTION
inline void Spu::key_on_mask(uint64_t mask) {
    for (int v = 0; v < kVoiceCount; ++v) {
        if (!(mask & (uint64_t(1) << v))) continue;
        voices_[v].reset();
        voices_[v].key_on(regs_[v], ram_);
        adsr_[v] = {};
        adsr_[v].key_on(adsr_params_[v]);
    }
}

inline void Spu::key_off_mask(uint64_t mask) {
    for (int v = 0; v < kVoiceCount; ++v)
        if (mask & (uint64_t(1) << v)) adsr_[v].key_off();
}

inline size_t Spu::dma_write(uint32_t addr, std::span<const uint8_t> data) {
    const uint32_t end =
        addr + static_cast<uint32_t>(data.size()) > kSpuRamSize
            ? kSpuRamSize
            : addr + static_cast<uint32_t>(data.size());
    const size_t n = end > addr ? end - addr : 0;
    for (size_t i = 0; i < n; ++i) {
        ram_.data[addr + i] = data[i];
        // IRQ9 compares every transferred byte against the match address.
        if ((control_ & 0x40) &&
            addr + static_cast<uint32_t>(i) ==
                static_cast<uint32_t>(irq_addr_word_) << 3)
            irq_flag_ = true;
    }
    return n;
}

inline void Spu::set_cd_input(int16_t left, int16_t right) {
    cd_in_l_ = left;
    cd_in_r_ = right;
}
//@LABS-STUB
// TODO(11): key events + DMA.
//   key_on_mask(): for every set bit reset + key_on that voice and restart
//     its envelope from the cached params.
//   key_off_mask(): send key_off to those envelopes only.
//     Also route writes to KEY_ON (1C0-1C2) and KEY_OFF (1CC-1CE) through
//     the key masks, shifting the 16-bit value by (offset-base)*16.

//   dma_write(): copy up to the end of SPU RAM, returning bytes copied;
//     while copying, raise the IRQ flag for any byte landing exactly on
//     irq_addr_word_ << 3 when control.6 (IRQ enable) is set.
inline void Spu::key_on_mask(uint64_t) {
    // TODO(11): start requested voices.
}

inline void Spu::key_off_mask(uint64_t) {
    // TODO(11): release requested envelopes.
}

inline size_t Spu::dma_write(uint32_t, std::span<const uint8_t>) {
    // TODO(11): fill SPU RAM, watch the IRQ compare address.
    return 0;
}

inline void Spu::set_cd_input(int16_t, int16_t) {
    // TODO(11): latch the CD decoder's stereo output pair.
}
//@LABS-END

//@LABS-BEGIN 12
//@LABS-SOLUTION
inline void Spu::render(int frames, std::vector<int16_t>& out) {
    auto scale = [](int64_t s, uint16_t vol) {
        int64_t v = (s * static_cast<int64_t>(vol)) >> 14;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        return static_cast<int16_t>(v);
    };
    for (int f = 0; f < frames; ++f) {
        int32_t acc_l = 0, acc_r = 0;
        for (int v = 0; v < kVoiceCount; ++v) {
            if (!voices_[v].active()) continue;
            const int env = adsr_[v].tick();
            const int32_t s = voices_[v].tick(ram_);
            // A finished envelope silences the voice even mid-stream.
            if (env == 0 && adsr_[v].phase() == AdsrPhase::Off) continue;
            const int32_t voiced = (s * env) >> 15;
            acc_l += (voiced * static_cast<int32_t>(regs_[v].vol_left)) >> 14;
            acc_r += (voiced * static_cast<int32_t>(regs_[v].vol_right)) >> 14;
            // IRQ9 address match on voice decode position.
            if ((control_ & 0x40) &&
                voices_[v].current_addr() ==
                    static_cast<uint32_t>(irq_addr_word_) << 3)
                irq_flag_ = true;
        }
        // CD-audio mixed input path.
        acc_l += (static_cast<int32_t>(cd_in_l_) *
                  static_cast<int32_t>(cd_vol_l_)) >> 14;
        acc_r += (static_cast<int32_t>(cd_in_r_) *
                  static_cast<int32_t>(cd_vol_r_)) >> 14;
        out.push_back(scale(acc_l, main_vol_l_));
        out.push_back(scale(acc_r, main_vol_r_));
    }
}
//@LABS-STUB
// TODO(12): mixing core. For every active voice: advance its envelope,
// pull one resampled stream sample, weight by env>>15 and the voice
// volume (>>14). Add the CD input scaled by the CD volumes, then scale
// both accumulators by the main volume and push interleaved L/R pairs.
// Also watch each voice's decode address against the IRQ compare address.
inline void Spu::render(int, std::vector<int16_t>&) {
    // TODO(12): sum voices + CD input into the output buffer.
}
//@LABS-END

}  // namespace spu
