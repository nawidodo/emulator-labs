#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "system.hpp"

using namespace gba;

TEST(scheduler, fifo_ordering_and_ties) {
    Scheduler s;
    std::vector<int> order;
    s.schedule(10, [&] { order.push_back(2); });
    s.schedule(5, [&] { order.push_back(1); });
    s.schedule(10, [&] { order.push_back(3); });  // same time as first
    s.run_until(100);
    EXPECT_EQ(order.size(), 3u);
    EXPECT_TRUE(order[0] == 1 && order[1] == 2 && order[2] == 3);  // seq tie-break
}

TEST(scheduler, dma_burst_defers_events) {
    Scheduler s;
    std::vector<u64> fired_at;
    s.schedule(50, [&] { fired_at.push_back(s.now); });
    s.run_until(10);
    // A DMA burst starts at cycle 10 and steals 100 cycles.
    s.begin_dma_burst(10, 100);
    s.run_until(200);
    EXPECT_EQ(fired_at.size(), 1u);
    EXPECT_EQ(fired_at[0], 110u);  // deferred to the end of the burst
}

TEST(scheduler, next_overflow_math) {
    EXPECT_EQ(next_overflow_in(0xFFFF, 1), 1u);
    EXPECT_EQ(next_overflow_in(0xFFF0, 16), 256u);  // (0x10000-0xFFF0)*16
    EXPECT_EQ(next_overflow_in(0, 1024), 0x10000ull * 1024);
}

TEST(video, cadence) {
    u64 cyc;
    VideoEvent kind;
    next_video_event(0, &cyc, &kind);
    EXPECT_TRUE(cyc == kHblankStart && kind == VideoEvent::HBlankStart);
    next_video_event(kHblankStart, &cyc, &kind);
    EXPECT_TRUE(cyc == kCyclesPerLine && kind == VideoEvent::LineStart);
    next_video_event(u64(kVblankLine - 1) * kCyclesPerLine + kHblankStart,
                     &cyc, &kind);
    EXPECT_TRUE(cyc == u64(kVblankLine) * kCyclesPerLine &&
                kind == VideoEvent::VBlankStart);
}

namespace {

// Script: timer0 (prescaler 64, reload near top) enabled at cycle 0 via a
// write, and an immediate DMA copying 4 halfwords.
void build_system(HWSystem& sys, std::vector<std::string>& order_hook) {
    sys.tm[0].reload = 0xFFFB;              // period 5 ticks
    sys.tm[0].counter = 0xFFFB;
    sys.tm[0].control = u16(0x80 | 0 | 1 << 6);  // enable, presc 1, IRQ on
    for (int i = 0; i < 4; ++i)
        sys.bus.ewram[i] = u8(i + 1);
    DmaRegs d;
    d.sad = 0x02000000;
    d.dad = 0x02000100;
    d.count = 4;
    d.control = 0x8000;
    sys.ch[0] = d;
    (void)order_hook;
}

}  // namespace

TEST(system, timer_overflows_are_periodic_and_logged) {
    HWSystem sys;
    build_system(sys, sys.trace);
    sys.schedule_timer(0);
    sys.sched.run_until(12);
    int tmr_logs = 0;
    for (const auto& l : sys.trace)
        if (l.find("op=tmr") != std::string::npos) ++tmr_logs;
    // Period 5: overflows at cycles 5 and 10.
    EXPECT_EQ(tmr_logs, 2);
    bool saw_irq = false;
    for (const auto& l : sys.trace)
        if (l.find("op=irq src=t0 cyc=5") != std::string::npos) saw_irq = true;
    EXPECT_TRUE(saw_irq);
}
