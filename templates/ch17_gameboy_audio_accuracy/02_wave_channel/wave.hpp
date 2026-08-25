// wave.hpp — DMG wave (channel 3): 32 4-bit samples played back at a
// programmable rate.
//
// Committed chapter conventions:
//   * Timer period = (2048 - freq) * 2 T-cycles per sample position.
//   * Trigger (NR34 bit 7) resets BOTH position and timer to 0 — the
//     channel always replays from the start of wave RAM.
//   * Wave RAM is 16 bytes x two nibbles = 32 samples of 4 bits; even
//     positions take the HIGH nibble ((byte >> 4) & 0xF), odd positions
//     the LOW nibble (byte & 0xF).
//   * NR32 volume code bits 5-6: 0 = mute, 1 = 100% (>>0), 2 = 50% (>>1),
//     3 = 25% (>>2).
//   * Output is gated by NR30 bit 7 (DAC power); disabled/DAC-off
//     channels output 0.
#pragma once

#include <cstdint>

namespace gbaudio {

constexpr int kWavePositions = 32;

class WaveChannel {
public:
    // ---- register storage ----
    uint8_t nr30 = 0;   // DAC enable bit7
    uint8_t nr32 = 0;   // volume code bits5-6
    uint16_t freq = 0;  // NR33 | NR34 low 3 bits (11-bit)
    bool enabled = false;
    uint8_t waveRam[16] = {0};  // FF30-FF3F, preserved across power cycles

    bool dacEnabled() const { return (nr30 & 0x80) != 0; }
    int volumeCode() const { return (nr32 >> 5) & 3; }
    uint32_t timerPeriod() const { return (2048u - freq) * 2u; }

    void writeNR30(uint8_t v) {
        nr30 = v;
        if (!dacEnabled()) enabled = false;  // killing the DAC silences ch3
    }
    void writeNR32(uint8_t v) { nr32 = v; }
    void writeWaveRam(int offset, uint8_t v) { waveRam[offset & 15] = v; }

    void writeNR33(uint8_t v) {
        freq = static_cast<uint16_t>((freq & 0x700) | v);
    }
    void writeNR34(uint8_t v) {
        freq = static_cast<uint16_t>((freq & 0xFF) | ((v & 0x07) << 8));
        if (v & 0x80) trigger();
    }

    // Trigger: restart playback from position 0 with a fresh timer.
    void trigger() {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        enabled = true;
        position = 0;
        timer = static_cast<int32_t>(timerPeriod());
//@LABS-STUB
        // TODO(1): enable the channel, rewind position to 0 and reload
        // the sample timer from timerPeriod() so position 0 plays for a
        // complete period.
//@LABS-END
    }

    // Advance the sample timer by `cycles` T-cycles while playing.
    void advance(int cycles) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if (!enabled) return;
        timer -= cycles;
        while (timer <= 0) {
            timer += static_cast<int32_t>(timerPeriod());
            position = (position + 1) & (kWavePositions - 1);
        }
//@LABS-STUB
        // TODO(2): while enabled, count the timer down; on expiry reload
        // it from timerPeriod() and step position forward mod 32.
        (void)cycles;
//@LABS-END
    }

    // Raw 4-bit sample at a position: high nibble on even positions,
    // low nibble on odd ones.
    int sampleNibble(int pos) const {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        const uint8_t byte = waveRam[(pos >> 1) & 15];
        return (pos & 1) ? (byte & 0xF) : ((byte >> 4) & 0xF);
//@LABS-STUB
        // TODO(3): decode wave RAM nibble for `pos` (even -> high nibble).
        (void)pos;
        return 0;
//@LABS-END
    }

    // Current output 0..15 after volume-code shifting and gating.
    int sample() const {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (!enabled || !dacEnabled()) return 0;
        static constexpr int kShift[4] = {4, 0, 1, 2};  // code 0 shifts out
        return sampleNibble(position) >> kShift[volumeCode()];
//@LABS-STUB
        // TODO(4): gate on enabled && dacEnabled(); apply the NR32 volume
        // code shift table {mute:4, 100%:0, 50%:1, 25%:2} to the raw
        // nibble at the current position.
        return 0;
//@LABS-END
    }

    int position = 0;      // current sample index 0..31
private:
    int32_t timer = 0;     // T-cycles until next position step
};
}  // namespace gbaudio
