// noise.hpp — DMG noise channel (channel 4): an LFSR driven at a
// programmable rate, scaled by the same envelope as the pulse channels.
//
// Committed chapter conventions:
//   * Divisor code table r = {8,16,32,48,64,80,96,112} indexed by NR43
//     bits 0-2; timer period = divisor << s where s = NR43 bits 4-7.
//     (Hardware codes 0..2 all behave as 8; this table encodes that.)
//   * The LFSR is 15 bits wide and reloads to 0x7FFF on trigger.
//   * One LFSR step, EXACT formulation (do not "simplify" it):
//       int x = (lfsr ^ (lfsr >> 1)) & 1;
//       lfsr = (lfsr >> 1) | (x << 14);
//       if width mode 7: set BOTH bit 6 and bit 14 to x
//   * Output = (~lfsr & 1) * volume — the inverted bit0 AFTER the shift,
//     scaled by the envelope volume (0 when the channel is disabled).
#pragma once

#include <cstdint>

namespace gbaudio {

constexpr int kNoiseLengthMax = 64;

constexpr uint8_t kNoiseDivisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};

class NoiseChannel {
public:
    // ---- register storage ----
    uint8_t nr41 = 0;   // length code bits0-5
    uint8_t nr42 = 0;   // initial volume bits4-7, increase bit3, period bits0-2
    uint8_t nr43 = 0;   // s bits4-7, width7 bit3, divisor code bits0-2
    bool lengthEnabled = false;

    bool enabled = false;
    int lengthCounter = 0;
    int envVolume = 0;
    uint16_t lfsr = 0x7FFF;

    uint8_t initialVolume() const { return static_cast<uint8_t>(nr42 >> 4); }
    bool envIncrease() const { return (nr42 & 0x08) != 0; }
    uint8_t envPeriod() const { return static_cast<uint8_t>(nr42 & 7); }
    bool dacEnabled() const { return (nr42 & 0xF8) != 0; }

    uint8_t shiftClock() const { return static_cast<uint8_t>(nr43 >> 4); }
    bool width7() const { return (nr43 & 0x08) != 0; }
    uint8_t divisorCode() const { return static_cast<uint8_t>(nr43 & 7); }

    void writeNR42(uint8_t v) {
        nr42 = v;
        if ((v & 0xF8) == 0) enabled = false;  // DAC power rule
    }
    void writeNR44(uint8_t v) {
        lengthEnabled = (v & 0x40) != 0;
        if (v & 0x80) trigger();
    }

    // Timer period in T-cycles between LFSR steps.
    uint32_t stepPeriod() const {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        return static_cast<uint32_t>(kNoiseDivisors[divisorCode()])
               << shiftClock();
//@LABS-STUB
        // TODO(1): divisor-table entry indexed by NR43 bits 0-2, shifted
        // left by NR43 bits 4-7.
        return 1;
//@LABS-END
    }

    // One LFSR step using the exact committed formulation above.
    void step() {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        const int x = (lfsr ^ (lfsr >> 1)) & 1;
        lfsr = static_cast<uint16_t>((lfsr >> 1) | (x << 14));
        if (width7()) {
            lfsr = x ? static_cast<uint16_t>(lfsr | 0x40)
                     : static_cast<uint16_t>(lfsr & ~0x40);
        }
//@LABS-STUB
        // TODO(2): XOR taps bit0/bit1, shift right, insert the result at
        // bit 14; in width-7 mode also copy it into bit 6.
//@LABS-END
    }

    // Trigger (NR44 bit 7): fresh LFSR (nonzero!), envelope and timer.
    void trigger() {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        enabled = true;
        if (lengthCounter == 0) lengthCounter = kNoiseLengthMax;
        lfsr = 0x7FFF;
        envVolume = initialVolume();
        envTimer = envPeriod();
        timer = static_cast<int32_t>(stepPeriod());
//@LABS-STUB
        // TODO(3): enable; reload length 0 -> 64; lfsr = 0x7FFF; reset
        // envelope volume/timer; reload step timer from stepPeriod().
//@LABS-END
    }

    // Advance by `cycles` T-cycles, running one LFSR step per expiry.
    void advance(int cycles) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (!enabled) return;
        timer -= cycles;
        while (timer <= 0) {
            timer += static_cast<int32_t>(stepPeriod());
            step();
        }
//@LABS-STUB
        // TODO(4): while enabled count the timer down to zero, reloading
        // from stepPeriod() and calling step() on each expiry.
        (void)cycles;
//@LABS-END
    }

    void lengthTick() {
        if (lengthEnabled && lengthCounter > 0 && --lengthCounter == 0)
            enabled = false;
    }

    void envelopeTick() {
        if (envPeriod() == 0) return;
        if (--envTimer == 0) {
            envTimer = envPeriod();
            if (envIncrease()) {
                if (envVolume < 15) ++envVolume;
            } else if (envVolume > 0) {
                --envVolume;
            }
        }
    }

    // Current output 0..15: inverted bit0 of the shifted LFSR times volume.
    int sample() const {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        if (!enabled || !dacEnabled()) return 0;
        return (~lfsr & 1) ? envVolume : 0;
//@LABS-STUB
        // TODO(5): disabled/DAC-off -> 0; else (~lfsr & 1) scaled by the
        // envelope volume.
        return 0;
//@LABS-END
    }

private:
    int envTimer = 0;
    int32_t timer = 8;  // T-cycles until next LFSR step
};
}  // namespace gbaudio
