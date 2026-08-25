// apu.hpp — complete 4-channel DMG APU: frame sequencer, register file,
// NR50/51/52 mixing and deterministic Bresenham downsampling to 44100 Hz.
//
// Committed chapter conventions:
//   * Clock domain: T-cycles at kTCycleHz = 4194304. The frame sequencer
//     advances one of its 8 steps every kFsStepTcycles = 8192 T-cycles
//     (512 Hz): length ticks on steps 0/2/4/6, sweep on 2/6, envelope on
//     step 7.
//   * Downsampling (deterministic Bresenham, no floating point phase):
//       acc += kSampleRate each T-cycle;
//       while (acc >= kTCycleHz) { acc -= kTCycleHz; emit(mix()); }
//   * Mixing: per ACTIVE channel (enabled && DAC powered) the digital
//     output 0..15 maps to analog = (out - 7.5) / 7.5 in [-1, 1]; inactive
//     channels contribute nothing. Sides sum per NR51, scale by
//     ((NR50 volume + 1) / 8) per side, then clamp to [-1,1] and multiply
//     by kFullScale = 12000 (documented headroom factor below int16 clip).
//   * Frame layout: s16le interleaved L,R.
//   * NR52 power off clears ALL registers (wave RAM is preserved, as on
//     hardware); while powered down only NR52 accepts writes. The chip
//     starts powered DOWN — programs must write FF26=0x80 first.
#pragma once

#include <cstdint>

#include "../01_square_channels/square.hpp"
#include "../02_wave_channel/wave.hpp"
#include "../03_noise_lfsr/noise.hpp"
#include "audio_ring.hpp"

namespace gbaudio {

struct StereoSample {
    int16_t l = 0;
    int16_t r = 0;
};

class Apu {
public:
    static constexpr uint32_t kTCycleHz = 4194304u;
    static constexpr uint32_t kSampleRate = 44100u;
    static constexpr uint32_t kFsStepTcycles = 8192u;
    static constexpr uint32_t kFrameTcycles = 70224u;
    static constexpr double kDacMidpoint = 7.5;
    static constexpr double kFullScale = 12000.0;

    SquareChannel ch1{true};   // pulse with sweep
    SquareChannel ch2{false};  // plain pulse
    WaveChannel wave;
    NoiseChannel noise;
    uint8_t nr50 = 0;  // R volume bits4-6, L volume bits0-2
    uint8_t nr51 = 0;  // R ch1..ch4 bits4-7, L ch1..ch4 bits0-3
    uint8_t nr52 = 0;  // bit7 power; starts OFF

    size_t emittedSamples = 0;

    explicit Apu(S16RingBuffer& ring) : ring_(ring) {}

    // Power off (NR52 bit 7 cleared): wipe everything but wave RAM.
    void powerOff() {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        nr50 = 0;
        nr51 = 0;
        nr52 = 0;
        ch1 = SquareChannel{true};
        ch2 = SquareChannel{false};
        noise = NoiseChannel{};
        wave.nr30 = 0;
        wave.nr32 = 0;
        wave.freq = 0;
        wave.enabled = false;
        wave.position = 0;  // waveRam deliberately preserved (hardware)
//@LABS-STUB
        // TODO(1): zero nr50/nr51/nr52 and reset all four channels to
        // their power-on state WITHOUT touching wave.waveRam.
//@LABS-END
    }

    // Register write dispatch for FF10-FF3F.
    void writeReg(uint16_t addr, uint8_t v) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if (addr >= 0xFF30 && addr <= 0xFF3F) {  // always writable
            wave.writeWaveRam(addr - 0xFF30, v);
            return;
        }
        if (addr < 0xFF10 || addr > 0xFF26) return;
        if (!(nr52 & 0x80)) {  // powered down: only NR52 listens
            if (addr == 0xFF26 && (v & 0x80)) nr52 = 0x80;
            return;
        }
        switch (addr) {
            case 0xFF10: ch1.writeNR10(v); break;
            case 0xFF11: ch1.writeNR11(v); break;
            case 0xFF12: ch1.writeNR12(v); break;
            case 0xFF13: ch1.writeNR13(v); break;
            case 0xFF14: ch1.writeNR14(v); break;
            case 0xFF16: ch2.writeNR11(v); break;
            case 0xFF17: ch2.writeNR12(v); break;
            case 0xFF18: ch2.writeNR13(v); break;
            case 0xFF19: ch2.writeNR14(v); break;
            case 0xFF1A: wave.writeNR30(v); break;
            case 0xFF1B: break;  // NR31 length code: not modeled this chapter
            case 0xFF1C: wave.writeNR32(v); break;
            case 0xFF1D: wave.writeNR33(v); break;
            case 0xFF1E: wave.writeNR34(v); break;
            case 0xFF20: noise.nr41 = v & 0x3F; break;
            case 0xFF21: noise.writeNR42(v); break;
            case 0xFF22: noise.nr43 = v; break;
            case 0xFF23: if (v & 0x80) noise.trigger(); break;
            case 0xFF24: nr50 = v; break;
            case 0xFF25: nr51 = v; break;
            case 0xFF26: if (!(v & 0x80)) powerOff(); break;
            default: break;
        }
//@LABS-STUB
        // TODO(2): route writes per the map in SPEC.md — wave RAM
        // FF30-FF3F always; when powered down accept only FF26 bit7 set;
        // otherwise dispatch FF10-FF26 across ch1/ch2/wave/noise/NR50/
        // NR51/NR52 (FF15/FF1B ignored, FF26 clear -> powerOff()).
        (void)addr;
        (void)v;
//@LABS-END
    }

    // One frame-sequencer step (called every 8192 T-cycles).
    void fsStep(int step) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        const int s = step & 7;
        if ((s & 1) == 0) {  // 0/2/4/6: length at 256 Hz
            ch1.lengthTick();
            ch2.lengthTick();
            noise.lengthTick();
        }
        if (s == 2 || s == 6) ch1.sweepTick();  // sweep at 128 Hz
        if (s == 7) {                           // envelope at 64 Hz
            ch1.envelopeTick();
            ch2.envelopeTick();
            noise.envelopeTick();
        }
//@LABS-STUB
        // TODO(3): dispatch frame-sequencer step s = step & 7 — lengths
        // on even steps, sweep on 2 and 6, envelopes on 7.
        (void)step;
//@LABS-END
    }

    // Mix one output frame from current channel state.
    StereoSample mix() const {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        double l = 0.0, r = 0.0;
        const auto add = [&](bool active, int out, uint8_t rbit,
                             uint8_t lbit) {
            if (!active) return;  // dead channels leave the DAC centered
            const double a =
                (static_cast<double>(out) - kDacMidpoint) / kDacMidpoint;
            if (nr51 & rbit) r += a;
            if (nr51 & lbit) l += a;
        };
        add(ch1.enabled && ch1.dacEnabled(), ch1.sample(), 0x10, 0x01);
        add(ch2.enabled && ch2.dacEnabled(), ch2.sample(), 0x20, 0x02);
        add(wave.enabled && wave.dacEnabled(), wave.sample(), 0x40, 0x04);
        add(noise.enabled && noise.dacEnabled(), noise.sample(), 0x80, 0x08);
        l *= static_cast<double>((nr50 & 7) + 1) / 8.0;
        r *= static_cast<double>(((nr50 >> 4) & 7) + 1) / 8.0;
        const auto clamp = [](double x) {
            return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x);
        };
        return StereoSample{
            static_cast<int16_t>(clamp(l) * kFullScale),
            static_cast<int16_t>(clamp(r) * kFullScale)};
//@LABS-STUB
        // TODO(4): sum active channels per side using analog =
        // (out - 7.5) / 7.5, apply NR50 master volumes, clamp to
        // [-1,1] and scale by 12000 into an s16 pair.
        return StereoSample{};
//@LABS-END
    }

    // One T-cycle: channel timers, frame-sequencer cadence and the
    // Bresenham downsampler.
    void tick() {
        ch1.advance(1);
        ch2.advance(1);
        wave.advance(1);
        noise.advance(1);
        if (++fsCounter_ >= kFsStepTcycles) {
            fsCounter_ = 0;
            fsStep(fsStepIndex_++);
        }
        downAcc_ += kSampleRate;
        while (downAcc_ >= kTCycleHz) {
            downAcc_ -= kTCycleHz;
            const StereoSample s = mix();
            ring_.pushStereo(s.l, s.r);
            ++emittedSamples;
        }
    }

    void advance(uint32_t cycles) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        for (uint32_t t = 0; t < cycles; ++t) tick();
//@LABS-STUB
        // TODO(5): call tick() exactly `cycles` times.
        (void)cycles;
//@LABS-END
    }

private:
    S16RingBuffer& ring_;
    uint32_t fsCounter_ = 0;
    int fsStepIndex_ = 0;
    uint32_t downAcc_ = 0;
};

}  // namespace gbaudio
