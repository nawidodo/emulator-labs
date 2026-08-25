#define LABSTEST_MAIN
#include "labstest.hpp"
#include "system.hpp"
#include <cstddef>

#include <sstream>
#include <string>
#include <vector>

using namespace ps1sys;

// --- Mini assembler helpers (mirror core.hpp encoding) -------------------
static uint32_t lui(unsigned rt, uint16_t imm) { return rt << 16 | imm; }
static uint32_t ori(unsigned rt, uint16_t imm) {
    return 1u << 26 | rt << 16 | imm;
}
static uint32_t addiu(unsigned rt, unsigned rs, uint16_t imm) {
    return 2u << 26 | rs << 21 | rt << 16 | imm;
}
static uint32_t sw(unsigned rt, uint16_t off, unsigned rs) {
    return 3u << 26 | rs << 21 | rt << 16 | off;
}
static uint32_t lw(unsigned rt, uint16_t off, unsigned rs) {
    return 4u << 26 | rs << 21 | rt << 16 | off;
}
static uint32_t bnez(unsigned rs, int offs) {
    return 5u << 26 | rs << 21 | (static_cast<uint32_t>(offs) & 0xFFFFu);
}
static uint32_t j(uint32_t target) { return 6u << 26 | target; }
static uint32_t halt() { return 7u << 26; }
static uint32_t andi(unsigned rt, unsigned rs, uint16_t imm) {
    return 8u << 26 | rs << 21 | rt << 16 | imm;
}

template <class... W>
static void boot(System<sched::Scheduler>& sys, W... words) {
    const uint32_t prog[] = {words...};
    sys.load_rom(reinterpret_cast<const uint8_t*>(prog), sizeof(prog));
}

static size_t count(const Log& log, const std::string& needle) {
    size_t n = 0;
    for (const auto& line : log)
        if (line.find(needle) != std::string::npos) ++n;
    return n;
}

TEST(devices, dma_stall_pauses_cpu_until_drain) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    // Kick an 8-word DMA (8*6 = 48 cycles); the CPU must not execute
    // another instruction until the drain completes.
    boot(sys,
         lui(8, 0x1F80),          // 0: r8 = 0x1F800000
         addiu(9, 0, 8),          // 4: words = 8
         sw(9, 0x0C4, 8),         // 8: BCR = 8
         addiu(10, 0, 1),         // 12: start flag
         sw(10, 0x0C8, 8),        // 16: CHCR write -> DMA starts @20
         halt());                 // 20: must run only after the stall
    sys.run_until(1000);

    EXPECT_EQ(sys.trace().size(), size_t(6));
    // The CHCR store executes at cycle 16; the drain takes 8*6 = 48 more,
    // so the stalled HALT lands exactly at 64 — not at 20 as it would
    // without the stall.
    if (sys.trace().size() == 6) {
        EXPECT_EQ(sys.trace()[4], std::string("pc=00000010 op=0D0A00C8 "
                                              "cyc=16"));
        EXPECT_EQ(sys.trace()[5], std::string("pc=00000014 op=1C000000 "
                                              "cyc=64"));
    }
    EXPECT_EQ(count(sys.event_log(), "evt=dma_start"), size_t(1));
    // Completion latches INTC line 3 at the drain deadline.
    EXPECT_EQ(count(sys.event_log(), "cyc=64 evt=dma_done words=8"),
              size_t(1));
    EXPECT_EQ(count(sys.event_log(), "cyc=64 evt=latch line=3 src=dma"),
              size_t(1));
}

TEST(devices, dma_rejects_overlapping_start) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    sys.set_cpu_enabled(false);
    EXPECT_FALSE(sys.dma_ctrl().start(sys.sched(), 0, 0));
    EXPECT_TRUE(sys.dma_ctrl().start(sys.sched(), 0, 4));
    EXPECT_FALSE(sys.dma_ctrl().start(sys.sched(), 0, 4));  // still draining
    EXPECT_EQ(sys.dma_ctrl().done_time(), uint64_t(24));
}

TEST(devices, cd_completion_latches_irq2_after_latency) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    sys.set_cpu_enabled(false);
    EXPECT_TRUE(sys.cd_ctrl().read_sector(sys.sched(), 100));
    EXPECT_FALSE(sys.cd_ctrl().read_sector(sys.sched(), 101));  // busy
    sys.run_until(100 + kCdSectorCycles - 1);
    EXPECT_TRUE(count(sys.event_log(), "line=2 src=cd") == 0);
    sys.run_until(100 + kCdSectorCycles);
    EXPECT_TRUE(count(sys.event_log(),
                      "evt=cd_done lba=1") == 1);
    EXPECT_TRUE(count(sys.event_log(), "line=2 src=cd") == 1);
}

TEST(devices, gpu_pixel_ratio_uses_carrying_remainder) {
    Log log;
    Intc intc;
    intc.bind(&log);
    Gpu gpu;
    gpu.bind(&log, &intc);
    // 1024 pixels * 7 / 11 = 651.63.. -> 651 cycles, remainder 7 carries.
    EXPECT_EQ(gpu.cpu_cycles_for(1024), uint64_t(651));
    // (7 + 11*7) / 11 = 7 exactly; remainder stays 7.
    EXPECT_EQ(gpu.cpu_cycles_for(11), uint64_t(7));
    // (7 + 2048*7)/11 = 1303.9 -> 1303, remainder back to 7.
    EXPECT_EQ(gpu.cpu_cycles_for(2048), uint64_t(1303));
}

TEST(devices, gpu_gp0_queues_then_goes_idle_with_irq) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    sys.set_cpu_enabled(false);
    sys.store32(kIMask, 1u << kLineGpu);
    sys.store32(kGpuGp1, 0x04000001u);  // IRQ when queue drains
    sys.store32(kGpuGp0, 1100u);        // 1100 px = exactly 700 cycles
    EXPECT_TRUE(sys.gpu_ctrl().busy());
    EXPECT_NE(sys.load32(kGpuGp0) & 0x10000000u, 0u);  // STAT bit 28
    sys.run_until(350);
    EXPECT_TRUE(sys.gpu_ctrl().busy());
    sys.run_until(700);
    EXPECT_FALSE(sys.gpu_ctrl().busy());
    EXPECT_EQ(sys.load32(kGpuGp0), 0u);
    EXPECT_TRUE(count(sys.event_log(), "cyc=700 evt=gpu_idle") == 1);
    EXPECT_TRUE(count(sys.event_log(), "line=1 src=gpu") == 1);
}

TEST(devices, spu_sample_period_is_exactly_768_cycles) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    sys.set_cpu_enabled(false);
    sys.store32(kSpuCtrl, 1);
    sys.run_until(5 * kSpuSampleCycles);
    EXPECT_EQ(sys.spu_ctrl().sample_index(), uint64_t(5));
    EXPECT_EQ(count(sys.event_log(), "line=9 src=spu"), size_t(5));
    sys.run_until(5 * kSpuSampleCycles + kSpuSampleCycles - 1);
    EXPECT_EQ(sys.spu_ctrl().sample_index(), uint64_t(5));  // not early
    sys.run_until(6 * kSpuSampleCycles);
    EXPECT_EQ(sys.spu_ctrl().sample_index(), uint64_t(6));
}

TEST(devices, intc_mask_and_ack_semantics) {
    Log log;
    Intc intc;
    intc.bind(&log);
    intc.assert(10, kLineCd, "cd");
    EXPECT_EQ(intc.status() & (1u << kLineCd), 1u << kLineCd);
    EXPECT_EQ(intc.asserted(), 0u);           // masked out
    intc.set_mask(1u << kLineCd);
    EXPECT_EQ(intc.asserted(), 1u << kLineCd);
    intc.assert(11, kLineCd, "cd");           // re-raise logs another line
    EXPECT_EQ(count(log, "latch"), size_t(2));
    intc.ack(1u << kLineCd);
    EXPECT_EQ(intc.status(), 0u);
}

TEST(boot, polled_handshake_program_reaches_milestones) {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    // r8 = 0x1F800000; mask GPU|CD|SPU; milestone 1; SPU on; CD read;
    // spin until I_STAT bit2; ack it; milestone 2; HALT.
    boot(sys,
         lui(8, 0x1F80),                    // 0
         lui(10, 0x0000),                   // 1
         ori(10, 0x0206),                   // 2: lines 9|2|1
         sw(10, 0x074, 8),                  // 3: I_MASK
         addiu(12, 0, 1),                   // 4
         sw(12, 0xFF0, 8),                  // 5: milestone 1
         sw(12, 0xC00, 8),                  // 6: SPU_CTRL = 1
         sw(12, 0x800, 8),                  // 7: CD_CMD (kick @28)
         lw(11, 0x070, 8),                  // 8: poll I_STAT
         andi(11, 11, 0x004),               // 9
         bnez(11, 1),                       // 10 -> 12
         j(8),                              // 11
         sw(11, 0x070, 8),                  // 12: ack bit2
         addiu(12, 0, 2),                   // 13
         sw(12, 0xFF0, 8),                  // 14: milestone 2
         halt());                           // 15
    sys.run_until(60000);

    EXPECT_TRUE(sys.halted());
    // CD kicked at cycle 28 -> completion at 28 + 19968 = 19996. The poll
    // loop re-reads I_STAT every 16 cycles; FIFO dispatch guarantees the
    // cd_done event (earlier seq) beats the same-instant CPU event.
    EXPECT_TRUE(count(sys.event_log(), "cyc=19996 evt=cd_done lba=1") == 1);
    EXPECT_TRUE(count(sys.event_log(), "evt=milestone val=1") == 1);
    EXPECT_TRUE(count(sys.event_log(), "evt=milestone val=2") == 1);
    EXPECT_TRUE(count(sys.event_log(), "evt=halt") == 1);
}
