#pragma once
// Simplified APU: CPU<->APU communication ports $2140-$2147, a 64 KB APRAM
// fed by uploaded blocks, and a stripped DSP (per-voice volume, master
// volume, echo permanently DISABLED).
//
// Communication ports (real hardware, Anomie's register doc):
//   $2140/$2141, $2142/$2143, $2144/$2145, $2146/$2147 -- four DUAL 8-bit
//   ports; the CPU side and the SPC700 side each see their own registers
//   and use them as a mailbox with handshakes on $2140/$2142.
//
// SIMPLIFIED PROTOCOL (deterministic, no SPC700 program needed):
//   1. CPU writes $2140 = 0xA0 | slot        (handshake, slot 0..15)
//   2. CPU writes $2141 = length             (bytes to follow, 0..255)
//   3. CPU writes `length` data bytes, rotating across $2141,$2142,$2143
//      (the rotation is visible in cpu_read(); the APU consumes values in
//      write order regardless of which port carried them)
//   4. consume() drains queued writes through a small state machine and
//      commits each finished block into APRAM at slot * 4096.
//
// The APU never sees a byte twice and never misses one: every cpu_write is
// queued. Real hardware instead relies on timed SPC700 polling loops; we
// replace that with an explicit FIFO so tests are exact.
//
// DSP SIMPLIFICATION: each voice is a 16-bit sample scaled by an 8-bit
// per-voice volume; the mix is scaled by a 7-bit master volume. Echo is
// disabled at baseline (documented): no echo buffer, no feedback path.
#include <array>
#include <cstdint>
#include <deque>
#include <span>

namespace snesdma {

inline constexpr size_t kApramSize = 64 * 1024;
inline constexpr size_t kSlotBytes = 4096;
inline constexpr int kApuSlots = 16;
inline constexpr uint8_t kPortHandshake = 0;  // $2140 offset
inline constexpr uint8_t kHandshakeBase = 0xA0;

class Apu {
public:
    // CPU-side write into $2140-$2147 (offset 0-7). Always queues the event;
    // the APU core sees it on the next consume().
    void cpu_write(uint8_t offset, uint8_t value) {
        ports_[offset % 8] = value;
        pending_.push_back({uint8_t(offset % 8), value});
    }

    // Last value the CPU wrote to each port ($2140-$2147).
    uint8_t cpu_read(uint8_t offset) const { return ports_[offset % 8]; }

    // APU core poll: drains queued writes through the upload state machine.
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    void consume() {
        while (!pending_.empty()) {
            const auto [offset, value] = pending_.front();
            pending_.pop_front();
            switch (rx_) {
                case Rx::Idle:
                    if (offset == kPortHandshake &&
                        (value & 0xF0) == kHandshakeBase) {
                        slot_ = value & 0x0F;
                        rx_ = Rx::Length;
                    }
                    break;
                case Rx::Length:
                    length_ = value;
                    got_ = 0;
                    dest_ = size_t(slot_) * kSlotBytes;
                    rx_ = length_ == 0 ? Rx::Idle : Rx::Data;
                    break;
                case Rx::Data:
                    ram_[dest_ + got_] = value;
                    ++got_;
                    if (got_ == length_) rx_ = Rx::Idle;
                    break;
            }
        }
    }
    //@LABS-STUB
    // TODO(1): drain the pending write queue. Idle + handshake on $2140
    // (high nibble 0xA) selects the slot; the next queued byte is the
    // length; Data-state bytes land in APRAM at slot*4096 until done.
    void consume() {}
    //@LABS-END

    std::span<const uint8_t> apram() const { return ram_; }

    bool idle() const { return rx_ == Rx::Idle && pending_.empty(); }

private:
    enum class Rx { Idle, Length, Data };

    std::array<uint8_t, 8> ports_{};
    std::deque<std::pair<uint8_t, uint8_t>> pending_;
    std::array<uint8_t, kApramSize> ram_{};
    Rx rx_ = Rx::Idle;
    uint8_t slot_ = 0;
    uint8_t length_ = 0;
    uint16_t got_ = 0;
    size_t dest_ = 0;
};

// ---------------------------------------------------------------------------
// Simplified DSP.
// ---------------------------------------------------------------------------

struct DspVoice {
    int16_t sample = 0;  // 15-bit-class sample, stored int16
    uint8_t volume = 0;  // per-voice volume, 0..255
};

class Dsp {
public:
    DspVoice voices[8];
    uint8_t master_volume = 127;  // MAINVOL-style 7-bit attenuation

    // One voice contribution: arithmetic shift keeps negative samples
    // symmetric ((s*v)>>8 floors toward -inf, documented behaviour).
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    int32_t scale_voice(int voice) const {
        return (int32_t(voices[voice].sample) * voices[voice].volume) >> 8;
    }
    //@LABS-STUB
    // TODO(2): return (sample * volume) >> 8 for the given voice, using an
    // ARITHMETIC shift so negative samples scale correctly.
    int32_t scale_voice(int /*voice*/) const { return 0; }  // wrong on purpose
    //@LABS-END

    // Final mix: sum all voices, clamp to int16, scale by master volume,
    // clamp again. Echo would sit between the two clamps on real hardware;
    // this baseline has it DISABLED (no echo buffer exists at all).
    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    int16_t mix() const {
        int32_t sum = 0;
        for (const auto& v : voices) {
            sum += (int32_t(v.sample) * v.volume) >> 8;
        }
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        int32_t out = (sum * int32_t(master_volume)) >> 7;
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        return int16_t(out);
    }
    //@LABS-STUB
    // TODO(3): sum every scaled voice, clamp to [-32768, 32767], multiply
    // by master_volume, shift right by 7, clamp once more, return int16.
    int16_t mix() const { return 0; }  // wrong on purpose: silence forever
    //@LABS-END
};

}  // namespace snesdma
