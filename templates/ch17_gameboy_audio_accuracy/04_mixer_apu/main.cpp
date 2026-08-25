// Tests for exercise 04: frame sequencer, register file, mixing and
// deterministic downsampling. PCM-hash expectations were produced by the
// reference solution (run twice, byte-identical — see provenance.md).
#define LABSTEST_MAIN
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstddef>

#include "labstest.hpp"
#include "apu.hpp"

using gbaudio::Apu;
using gbaudio::S16RingBuffer;

namespace {

uint64_t fnv1a64(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

// Drains the APU ring after `frames` GB frames and returns its FNV-1a-64.
template <size_t Frames>
uint64_t pcmHash(Apu& apu, S16RingBuffer& ring) {
    apu.advance(Frames * gbaudio::Apu::kFrameTcycles);
    std::vector<int16_t> pcm(ring.size());
    ring.copyOut(pcm.data());
    return fnv1a64(pcm.data(), pcm.size() * sizeof(int16_t));
}

void powerOnDefaults(Apu& apu) {
    apu.writeReg(0xFF26, 0x80);  // power on
    apu.writeReg(0xFF24, 0x77);  // master volumes max
}

}  // namespace

TEST(apu_power, off_clears_everything_but_wave_ram) {
    S16RingBuffer ring;
    Apu apu{ring};
    powerOnDefaults(apu);
    apu.writeReg(0xFF30 + 3, 0x5A);  // a wave RAM byte worth keeping
    apu.writeReg(0xFF11, 0x80);
    apu.writeReg(0xFF12, 0xF0);
    apu.writeReg(0xFF14, 0x86);
    EXPECT_TRUE(apu.ch1.enabled);

    apu.writeReg(0xFF26, 0x00);  // power off
    EXPECT_EQ(apu.nr52, 0);
    EXPECT_EQ(apu.nr50, 0);
    EXPECT_EQ(apu.nr51, 0);
    EXPECT_FALSE(apu.ch1.enabled);
    EXPECT_EQ(apu.ch1.nrx1, 0);
    EXPECT_EQ(apu.ch1.nrx2, 0);
    EXPECT_EQ(apu.ch1.freq, 0);
    EXPECT_EQ(apu.noise.lfsr, 0x7FFF);  // reset value of a fresh channel
    EXPECT_EQ(apu.wave.waveRam[3], 0x5A);  // hardware preserves wave RAM

    // Powered down: register writes are ignored except power-on.
    apu.writeReg(0xFF24, 0x12);
    EXPECT_EQ(apu.nr50, 0);
    apu.writeReg(0xFF26, 0x80);
    EXPECT_EQ(apu.nr52, 0x80);
}

TEST(apu_downsample, emits_738_samples_per_frame) {
    S16RingBuffer ring;
    Apu apu{ring};
    apu.advance(gbaudio::Apu::kFrameTcycles);
    EXPECT_EQ(apu.emittedSamples, 738u);
    apu.advance(gbaudio::Apu::kFrameTcycles);
    EXPECT_EQ(apu.emittedSamples, 1476u);  // global Bresenham phase kept
}

TEST(apu_mix, silence_when_nothing_active) {
    S16RingBuffer ring;
    Apu apu{ring};
    apu.writeReg(0xFF26, 0x80);
    apu.writeReg(0xFF24, 0x77);
    const auto s = apu.mix();
    EXPECT_EQ(static_cast<int>(s.l), 0);
    EXPECT_EQ(static_cast<int>(s.r), 0);
}

TEST(apu_mix, nr51_routing_and_master_volume) {
    S16RingBuffer ring;
    Apu apu{ring};
    powerOnDefaults(apu);
    apu.writeReg(0xFF11, 0x80);   // 50% duty
    apu.writeReg(0xFF12, 0xF0);   // volume 15, envelope frozen
    apu.writeReg(0xFF13, 0xD6);
    apu.writeReg(0xFF14, 0x86);   // freq 1750 + trigger; pos 0 -> full scale
    // A fresh channel sits at dutyPos 0 whose 50% row bit is LOW, so the
    // digital output 0 maps to analog -1.0 -> full negative scale.
    apu.writeReg(0xFF25, 0x11);   // ch1 on both sides
    auto s = apu.mix();
    EXPECT_EQ(static_cast<int>(s.l), -12000);
    EXPECT_EQ(static_cast<int>(s.r), -12000);

    apu.writeReg(0xFF25, 0x10);   // right only
    s = apu.mix();
    EXPECT_EQ(static_cast<int>(s.l), 0);
    EXPECT_EQ(static_cast<int>(s.r), -12000);

    apu.writeReg(0xFF24, 0x40);   // left master volume 0 -> factor 1/8
    apu.writeReg(0xFF25, 0x01);   // left only
    s = apu.mix();
    EXPECT_EQ(static_cast<int>(s.l), -1500);  // -12000 / 8 exactly
    EXPECT_EQ(static_cast<int>(s.r), 0);
}

TEST(apu_pcm, pulse_envelope_config_hash) {
    S16RingBuffer ring(1 << 17);
    Apu apu{ring};
    powerOnDefaults(apu);
    apu.writeReg(0xFF25, 0xFF);
    apu.writeReg(0xFF11, 0x80);   // 50% duty
    apu.writeReg(0xFF12, 0xF7);   // volume 15, decay period 7
    apu.writeReg(0xFF13, 0xD6);
    apu.writeReg(0xFF14, 0x86);   // ~440 Hz carrier + trigger
    // Reference value from the solution run (twice, byte-identical).
    EXPECT_EQ(pcmHash<2>(apu, ring), 0x84BA0E7DBE15C45DULL);
}

TEST(apu_pcm, wave_pattern_config_hash) {
    S16RingBuffer ring(1 << 17);
    Apu apu{ring};
    powerOnDefaults(apu);
    apu.writeReg(0xFF25, 0x44);   // ch3 on both sides
    static const uint8_t ram[16] = {0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A,
                                    0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4,
                                    0xC3, 0xD2, 0xE1, 0xF0};
    for (int i = 0; i < 16; ++i) apu.writeReg(0xFF30 + i, ram[i]);
    apu.writeReg(0xFF1A, 0x80);   // DAC on
    apu.writeReg(0xFF1C, 0x20);   // 100% volume code
    apu.writeReg(0xFF1D, 0x54);   // freq 100
    apu.writeReg(0xFF1E, 0x84);   // trigger
    EXPECT_EQ(pcmHash<1>(apu, ring), 0x169A8728693FBD9DULL);
}

TEST(apu_pcm, noise_burst_config_hash) {
    S16RingBuffer ring(1 << 17);
    Apu apu{ring};
    powerOnDefaults(apu);
    apu.writeReg(0xFF25, 0x88);   // ch4 on both sides
    apu.writeReg(0xFF21, 0xC2);   // volume 12, decay period 2
    apu.writeReg(0xFF22, 0x34);   // s=3, divisor code 4, width 15
    apu.writeReg(0xFF23, 0x80);   // trigger
    EXPECT_EQ(pcmHash<1>(apu, ring), 0x941B79ED77D32665ULL);
}

// The coding-test register sequence documented verbatim in
// 99_coding_test/CODING_TEST.md; the hidden manifest hashes this exact
// configuration through the student's runner.
TEST(coding_test, unseen_config_setup) {
    S16RingBuffer ring;
    Apu apu{ring};
    struct W { uint16_t reg; uint8_t val; };
    const W writes[] = {
        {0xFF26, 0x80}, {0xFF24, 0x77}, {0xFF25, 0xBF},
        {0xFF11, 0x40}, {0xFF12, 0x74}, {0xFF13, 0xEE}, {0xFF14, 0x86},
        {0xFF16, 0x80}, {0xFF17, 0x83}, {0xFF18, 0x22}, {0xFF19, 0x85},
        {0xFF30, 0x01}, {0xFF31, 0x23}, {0xFF32, 0x45}, {0xFF33, 0x67},
        {0xFF34, 0x89}, {0xFF35, 0xAB}, {0xFF36, 0xCD}, {0xFF37, 0xEF},
        {0xFF38, 0xFE}, {0xFF39, 0xDC}, {0xFF3A, 0xBA}, {0xFF3B, 0x98},
        {0xFF3C, 0x76}, {0xFF3D, 0x54}, {0xFF3E, 0x32}, {0xFF3F, 0x10},
        {0xFF1A, 0x80}, {0xFF1C, 0x40}, {0xFF1D, 0x00}, {0xFF1E, 0x84},
        {0xFF21, 0xF2}, {0xFF22, 0x27}, {0xFF23, 0x80},
    };
    for (const W& w : writes) apu.writeReg(w.reg, w.val);

    // Configuration echoes: the runner applies the same records.
    EXPECT_EQ(apu.ch1.freq, 0x6EE);
    EXPECT_TRUE(apu.ch2.enabled);
    EXPECT_EQ(apu.wave.waveRam[5], 0xAB);
    EXPECT_EQ(apu.wave.freq, 0x400);  // FF1E=0x84 carries hi bits 100b
    EXPECT_EQ(apu.noise.nr43, 0x27);
    EXPECT_TRUE(apu.noise.enabled);
    EXPECT_EQ(apu.nr51, 0xBF);

    const auto s = apu.mix();
    EXPECT_TRUE(s.l != 0 || s.r != 0);  // something is actually audible
}
