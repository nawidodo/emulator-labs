#pragma once
#include <cstdint>

#include "../02_voice/voice.hpp"

namespace spu {

// The SPU mixes at 44100 Hz; the 16-bit pitch register is a fixed-point
// multiple of that rate: 0x1000 == 44100 Hz ("1.0x"), 0x0800 == 22050 Hz,
// 0x2000 == 88200 Hz. Playback walks the ADPCM sample stream at
// `pitch / 4096` samples per output sample.
constexpr uint32_t kPitchOne = 0x1000;

// Fractional sample-position accumulator (kept as 12.20 fixed point).
class PitchStepper {
public:
    void reset();

    // Adds `pitch` units to the position; returns how many whole samples
    // the voice must consume from its stream this output tick.
    unsigned advance(uint16_t pitch);

    // Position between the last two consumed samples, 1/4096 units.
    uint32_t fraction() const { return (acc_ >> 8) & 0xFFF; }

private:
    uint32_t acc_ = 0;
};

//@LABS-BEGIN 5
//@LABS-SOLUTION
inline void PitchStepper::reset() { acc_ = 0; }

inline unsigned PitchStepper::advance(uint16_t pitch) {
    acc_ += static_cast<uint32_t>(pitch) << 8;  // scale to 12.20
    unsigned whole = acc_ >> 20;
    acc_ &= 0xFFFFF;
    return whole;
}
//@LABS-STUB
// TODO(5): keep a 12.20 fixed-point position; advance() adds pitch << 8
// and returns the number of whole samples crossed (bits above bit 19),
// keeping only the fractional remainder.
inline void PitchStepper::reset() {
    // TODO(5): zero the accumulator.
}

inline unsigned PitchStepper::advance(uint16_t) {
    // TODO(5): accumulate and count whole samples crossed.
    return 1;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
// Linear interpolation between the previous (s0) and current (s1) stream
// samples. NOTE: real SPU hardware interpolates with a 4-point gaussian
// FIR table; this course documents linear interpolation as an acceptable
// stand-in. Swapping in a gaussian table later only touches this function.
inline int16_t interpolate(int16_t s0, int16_t s1, uint32_t frac) {
    // frac in [0,4095]: result = s0 + (s1-s0)*frac/4096.
    const int32_t diff = static_cast<int32_t>(s1) - static_cast<int32_t>(s0);
    int32_t v = (static_cast<int32_t>(s0) << 12) +
                diff * static_cast<int32_t>(frac);
    v >>= 12;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return static_cast<int16_t>(v);
}
//@LABS-STUB
// TODO(6): linear interpolation: result = s0 + ((s1-s0)*frac)/4096,
// clamped to int16. Return s0 unchanged for now.
inline int16_t interpolate(int16_t s0, int16_t s1, uint32_t frac) {
    (void)s1;
    (void)frac;
    return s0;  // wrong on purpose
}
//@LABS-END

// A voice plus its pitch stepper: produces output samples at the fixed
// 44100 Hz mix rate regardless of the stream's playback ratio.
class ResampledVoice {
public:
    void reset();
    void key_on(const VoiceRegs& regs, const SpuRam& ram);

    // One output sample at 44100 Hz.
    int16_t tick(const SpuRam& ram);

    bool active() const { return voice_.active(); }
    uint32_t current_addr() const { return voice_.current_addr(); }
    const PitchStepper& stepper() const { return stepper_; }

private:
    Voice voice_;
    PitchStepper stepper_;
    int16_t prev_ = 0;
    int16_t curr_ = 0;
    bool primed_ = false;
};

//@LABS-BEGIN 7
//@LABS-SOLUTION
inline void ResampledVoice::reset() {
    voice_.reset();
    stepper_.reset();
    prev_ = curr_ = 0;
    primed_ = false;
}

inline void ResampledVoice::key_on(const VoiceRegs& regs, const SpuRam& ram) {
    voice_.key_on(regs);
    stepper_.reset();
    prev_ = curr_ = 0;
    // Prime the interpolation window with the stream's first sample.
    curr_ = voice_.tick(ram);
    prev_ = curr_;
    primed_ = true;
}

inline int16_t ResampledVoice::tick(const SpuRam& ram) {
    if (!primed_ || !voice_.active()) return 0;
    unsigned consume = stepper_.advance(voice_.pitch());
    while (consume-- > 0) {
        prev_ = curr_;
        curr_ = voice_.tick(ram);  // 0 once the stream has ended
    }
    return interpolate(prev_, curr_, stepper_.fraction());
}
//@LABS-STUB
// TODO(7): wire voice + stepper together.
//   reset(): reset voice and stepper, clear the window, unprime.
//   key_on(): start the voice, reset the stepper, prime prev_/curr_ with
//     the stream's first sample.
//   tick(): inactive -> 0; otherwise ask the stepper how many stream
//     samples to consume (shifting the window), then interpolate between
//     prev_ and curr_ at the stepper's fraction.
inline void ResampledVoice::reset() {
    // TODO(7): clear all state.
}

inline void ResampledVoice::key_on(const VoiceRegs&, const SpuRam&) {
    // TODO(7): start playback with a primed window.
}

inline int16_t ResampledVoice::tick(const SpuRam&) {
    // TODO(7): consume stream samples and interpolate.
    return 0;
}
//@LABS-END

}  // namespace spu
