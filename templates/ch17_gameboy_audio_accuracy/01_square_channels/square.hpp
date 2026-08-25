// square.hpp — DMG pulse channel (channels 1 and 2).
//
// Committed chapter conventions (SPEC.md / LECTURE.md repeat these):
//   * Clock domain: T-cycles at 4194304 Hz. The frame sequencer ticks
//     length at 256 Hz, sweep at 128 Hz and envelope at 64 Hz.
//   * Duty waveforms are MSB-first rows selected by NRx1 bits 6-7:
//       0: 00000001 (0x01)   1: 00000011 (0x03)
//       2: 00001111 (0x0F)   3: 00111111 (0x3F)
//     sample bit = (form >> (7 - position)) & 1 with position in 0..7.
//   * Frequency timer period = (2048 - freq) * 4 T-cycles.
//   * A disabled channel outputs 0.
//   * Sweep model: candidates are computed one update ahead (at trigger
//     and after every applied update) and applied on the following sweep
//     tick. A candidate > 2047 disables the channel; a negative candidate
//     is discarded and leaves shadow/frequency untouched.
#pragma once

#include <cstdint>

namespace gbaudio {

constexpr int kSquareLengthMax = 64;

// MSB-first duty rows, indexed by NRx1 bits 6-7.
constexpr uint8_t kDutyForms[4] = {0x01, 0x03, 0x0F, 0x3F};

class SquareChannel {
public:
    SquareChannel() = default;
    explicit SquareChannel(bool withSweep) : hasSweep(withSweep) {}

    // ---- register storage + writes (FF10-FF14 ch1, FF16-FF19 ch2) ----
    uint8_t nr10 = 0;   // pace bits4-6, negate bit3, slope bits0-2
    uint8_t nrx1 = 0;   // duty bits6-7, length code bits0-5
    uint8_t nrx2 = 0;   // initial volume bits4-7, increase bit3, period bits0-2
    uint16_t freq = 0;  // NR13 | NR14 low 3 bits (11-bit)
    bool lengthEnabled = false;

    void writeNR10(uint8_t v) {
        if (!hasSweep) return;  // ch2 has no NR10; APU never routes it here
        nr10 = v;
    }
    void writeNR11(uint8_t v) { nrx1 = v; }
    void writeNR12(uint8_t v) {
        nrx2 = v;
        // DAC off (upper five bits zero) silences and disables the channel,
        // exactly as the hardware DAC-power rule does.
        if ((v & 0xF8) == 0) enabled = false;
    }
    void writeNR13(uint8_t v) {
        freq = static_cast<uint16_t>((freq & 0x700) | v);
    }
    void writeNR14(uint8_t v) {
        freq = static_cast<uint16_t>((freq & 0xFF) | ((v & 0x07) << 8));
        lengthEnabled = (v & 0x40) != 0;
        if (v & 0x80) trigger();
    }

    // ---- observable state ----
    bool enabled = false;
    int lengthCounter = 0;      // 0..64 (64 = "code 0 after reload")
    int dutyPos = 0;            // waveform position 0..7
    int envVolume = 0;          // current envelope volume 0..15
    uint16_t shadowFreq = 0;    // sweep shadow register
    int32_t pendingCandidate = 0;
    bool hasPendingCandidate = false;

    uint8_t pace() const { return static_cast<uint8_t>((nr10 >> 4) & 7); }
    bool sweepNegate() const { return (nr10 & 0x08) != 0; }
    uint8_t slope() const { return static_cast<uint8_t>(nr10 & 7); }
    uint8_t duty() const { return static_cast<uint8_t>(nrx1 >> 6); }
    uint8_t initialVolume() const { return static_cast<uint8_t>(nrx2 >> 4); }
    bool envIncrease() const { return (nrx2 & 0x08) != 0; }
    uint8_t envPeriod() const { return static_cast<uint8_t>(nrx2 & 7); }
    bool dacEnabled() const { return (nrx2 & 0xF8) != 0; }

    uint32_t freqPeriod() const { return (2048u - freq) * 4u; }

    // Trigger (NRx4 bit 7): reload everything, then run the immediate
    // first sweep calculation per hardware.
    void trigger() {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        enabled = true;
        if (lengthCounter == 0) lengthCounter = kSquareLengthMax;
        freqTimer = static_cast<int32_t>(freqPeriod());
        envVolume = initialVolume();
        envTimer = envPeriod();
        if (hasSweep) {
            shadowFreq = freq;
            sweepTimer = pace();
            hasPendingCandidate = false;
            if (pace() != 0) {
                const int32_t c = calcSweepCandidate();
                if (c > 2047) {
                    enabled = false;  // immediate overflow kills the channel
                } else {
                    pendingCandidate = c;
                    hasPendingCandidate = true;
                }
            }
        }
//@LABS-STUB
        // TODO(1): enable the channel; reload lengthCounter 0 -> 64;
        // reload freqTimer from freqPeriod(); reset envelope volume to
        // initialVolume() and envTimer to envPeriod(); when the channel
        // owns a sweep: shadowFreq = freq, sweepTimer = pace(), compute
        // the immediate first candidate and disable on overflow.
        (void)kSquareLengthMax;
//@LABS-END
    }

    // Advance the frequency timer by `cycles` T-cycles, walking the duty
    // position MSB-first through the 8-step waveform.
    void advance(int cycles) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        freqTimer -= cycles;
        while (freqTimer <= 0) {
            freqTimer += static_cast<int32_t>(freqPeriod());
            dutyPos = (dutyPos + 1) & 7;
        }
//@LABS-STUB
        // TODO(2): subtract `cycles` from freqTimer; each time it expires
        // reload it from freqPeriod() and step dutyPos forward mod 8.
        (void)cycles;
//@LABS-END
    }

    // Length tick (256 Hz, frame-sequencer steps 0/2/4/6).
    void lengthTick() {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (lengthEnabled && lengthCounter > 0 &&
            --lengthCounter == 0) {
            enabled = false;
        }
//@LABS-STUB
        // TODO(3): when length is enabled, decrement lengthCounter; hitting
        // zero disables the channel.
//@LABS-END
    }

    // Envelope tick (64 Hz, frame-sequencer step 7). Period 0 freezes the
    // volume; otherwise the timer counts down to a step toward 15 or 0.
    void envelopeTick() {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (envPeriod() == 0) return;
        if (--envTimer == 0) {
            envTimer = envPeriod();
            if (envIncrease()) {
                if (envVolume < 15) ++envVolume;
            } else {
                if (envVolume > 0) --envVolume;
            }
        }
//@LABS-STUB
        // TODO(4): implement the 64 Hz envelope walk described above;
        // clamp at both bounds.
//@LABS-END
    }

    // One sweep arithmetic candidate from the shadow frequency:
    // shadow +/- (shadow >> slope), sign chosen by NR10 negate mode.
    int32_t calcSweepCandidate() const {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        const int32_t delta = shadowFreq >> slope();
        return sweepNegate() ? static_cast<int32_t>(shadowFreq) - delta
                             : static_cast<int32_t>(shadowFreq) + delta;
//@LABS-STUB
        // TODO(5): return shadow +/- (shadow >> slope) per the negate bit.
        return 0;
//@LABS-END
    }

    // Sweep tick (128 Hz, frame-sequencer steps 2/6). Applies the pending
    // candidate computed one update earlier, then immediately computes the
    // next candidate (whose overflow disables the channel — the hardware's
    // "second update" overflow check). Negative candidates are discarded.
    void sweepTick() {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        if (!hasSweep || pace() == 0 || !hasPendingCandidate) return;
        if (--sweepTimer > 0) return;
        sweepTimer = pace();
        const int32_t c = pendingCandidate;
        hasPendingCandidate = false;
        if (c > 2047) {
            enabled = false;
            return;
        }
        if (c >= 0) {
            shadowFreq = static_cast<uint16_t>(c);
            freq = static_cast<uint16_t>(c);
        }
        pendingCandidate = calcSweepCandidate();
        hasPendingCandidate = true;
        if (pendingCandidate > 2047) enabled = false;
//@LABS-STUB
        // TODO(6): apply the pending candidate per the committed sweep
        // model (see header comment), then compute and store the next
        // candidate, disabling on overflow.
//@LABS-END
    }

    // Current output 0..15 (duty bit scaled by envelope volume).
    int sample() const {
//@LABS-BEGIN 7
//@LABS-SOLUTION
        if (!enabled) return 0;
        const uint8_t form = kDutyForms[duty()];
        const int bit = static_cast<int>((form >> (7 - dutyPos)) & 1);
        return bit ? envVolume : 0;
//@LABS-STUB
        // TODO(7): disabled channel -> 0; otherwise the MSB-first duty bit
        // at dutyPos selects between 0 and envVolume.
        return 0;
//@LABS-END
    }

private:
    bool hasSweep = true;       // ch1 yes, ch2 no
    int32_t freqTimer = 4;      // T-cycles until next duty step
    int envTimer = 0;           // countdown to next envelope step
    int sweepTimer = 0;         // sweep pace countdown
};
}  // namespace gbaudio
