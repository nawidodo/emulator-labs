#define LABSTEST_MAIN
#include "labstest.hpp"

#include <vector>

#include "machine.hpp"

using namespace nes24sync;

TEST(nes24sched, ppu_catches_up_three_dots_per_cpu_cycle) {
    Machine m;
    int dots0 = m.ppu.scanline * 341 + m.ppu.dot;
    m.cpu_tick();
    int dots1 = m.ppu.scanline * 341 + m.ppu.dot;
    EXPECT_EQ(dots1 - dots0, kPpuDotsPerCpu);
}

TEST(nes24dma, oam_dma_costs_513_cycles_from_even_alignment) {
    Machine m;  // cpu_cycle starts at 0 (even)
    for (int i = 0; i < 256; ++i)
        m.ram[(0x0200 + i) & 0x7FF] = uint8_t(i ^ 0x5A);
    uint64_t start = m.cpu_cycle;
    m.request_oam_dma(0x02);
    while (m.dma_active || m.dma_pending) m.cpu_tick();
    EXPECT_EQ(m.cpu_cycle - start, 513);
    for (int i = 0; i < 256; ++i)
        EXPECT_EQ(m.ppu.oam[size_t(i)], uint8_t(i ^ 0x5A));
}

TEST(nes24dma, odd_cycle_start_pays_one_extra_cycle) {
    Machine m;
    m.cpu_tick();               // now on an odd cycle boundary
    uint64_t start = m.cpu_cycle;
    m.request_oam_dma(0x03);
    while (m.dma_active || m.dma_pending) m.cpu_tick();
    EXPECT_EQ(m.cpu_cycle - start, 514);
}

TEST(nes24dma, dma_stalls_the_cpu_but_the_ppu_keeps_running) {
    Machine m;
    int scanline_before = m.ppu.scanline;
    int dots_before = m.ppu.scanline * 341 + m.ppu.dot;
    m.request_oam_dma(0x02);
    while (m.dma_active || m.dma_pending) m.cpu_tick();
    int dots_after = m.ppu.scanline * 341 + m.ppu.dot;
    // 513 CPU cycles x 3 dots advanced during the stall.
    EXPECT_EQ(dots_after - dots_before, 513 * kPpuDotsPerCpu);
    EXPECT_TRUE(dots_after != dots_before || scanline_before >= 0);
}

TEST(nes24frame, four_step_irq_fires_at_29829_unless_inhibited) {
    Machine m;
    for (uint64_t i = 0; i < 29829; ++i) m.cpu_tick();
    EXPECT_TRUE(m.apu.frame.irq);
    Machine b;
    b.apu.frame.write(0x40);    // inhibit
    for (uint64_t i = 0; i < 29829; ++i) b.cpu_tick();
    EXPECT_FALSE(b.apu.frame.irq);
}

TEST(nes24frame, five_step_mode_never_asserts_the_irq) {
    Machine m;
    m.apu.frame.write(0x80);    // 5-step
    for (int i = 0; i < 40000; ++i) m.cpu_tick();
    EXPECT_FALSE(m.apu.frame.irq);
    // Halves landed at 14913 and 37281: exactly two by cycle 40000.
    EXPECT_EQ(m.apu.frame.halves, 2);
}

TEST(nes24audio, one_sample_lands_per_cpu_cycle) {
    Machine m;
    for (int i = 0; i < 100; ++i) m.cpu_tick();
    EXPECT_EQ(m.audio.size(), size_t(100));
}
