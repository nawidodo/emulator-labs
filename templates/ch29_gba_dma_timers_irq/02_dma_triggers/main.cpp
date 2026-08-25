#define LABSTEST_MAIN
#include "labstest.hpp"
#include "dma_video.hpp"

using namespace gba;

namespace {

DmaRegs hblank_channel(int count, bool repeat) {
    DmaRegs r;
    r.sad = 0x02000000;
    r.dad = 0x04000000 + 0x100;  // some register target
    r.count = u16(count);
    // enable | timing=HBlank(2) | dst fixed
    r.control = u16(0x8000 | 2 << 12 | (repeat ? 1 << 9 : 0) | 3 << 5);
    return r;
}

}  // namespace

TEST(trigger, matching_and_once_semantics) {
    DmaRegs hb = hblank_channel(4, false);
    EXPECT_TRUE(dma_should_fire(hb, Trigger::HBlank, false));
    EXPECT_FALSE(dma_should_fire(hb, Trigger::HBlank, true));   // no repeat
    EXPECT_FALSE(dma_should_fire(hb, Trigger::VBlank, false));  // wrong trig

    DmaRegs rep = hblank_channel(4, true);
    EXPECT_TRUE(dma_should_fire(rep, Trigger::HBlank, true));   // re-arms

    DmaRegs off = hblank_channel(4, true);
    off.control &= ~0x8000u;
    EXPECT_FALSE(dma_should_fire(off, Trigger::HBlank, false));

    DmaRegs imm;
    imm.control = 0x8000;  // immediate
    EXPECT_FALSE(dma_should_fire(imm, Trigger::VBlank, false));
}

TEST(arbitration, lowest_number_first) {
    std::vector<int> order = arbitrate({3, 1, 2});
    EXPECT_EQ(order.size(), 3u);
    EXPECT_TRUE(order[0] == 1 && order[1] == 2 && order[2] == 3);
    order = arbitrate({2, 0});
    EXPECT_TRUE(order[0] == 0 && order[1] == 2);
}

TEST(fifo, refill_copies_four_words_and_reloads_dad) {
    Bus bus;
    for (int i = 0; i < 4; ++i)
        bus.wr32(0x02000000 + u32(i) * 4, u32(0xAA00 + i));
    DmaRegs r;
    r.sad = 0x02000000;
    r.dad = 0x040000A0 + 16;  // drifted base on purpose
    r.count = 4;
    r.control = u16(0x8000 | 3 << 12);  // special timing
    u64 cyc = fifo_refill(bus, r, 0x040000A0);
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(bus.rd32(0x040000A0 + u32(i) * 4), u32(0xAA00 + i));
    EXPECT_EQ(r.dad, 0x040000A0u);  // reloaded to base
    EXPECT_EQ(cyc, 32u);
}

TEST(video_event, once_vs_repeat_across_lines) {
    Bus bus;
    DmaRegs ch[4] = {};
    bool fired[4] = {false, false, false, false};
    ch[1] = hblank_channel(1, true);   // repeats every line
    ch[3] = hblank_channel(1, false);  // fires once

    auto a1 = run_video_event(bus, ch, fired, Trigger::HBlank,
                              kCyclesPerLine * 5 + kHblankStart);
    EXPECT_EQ(a1.size(), 2u);
    EXPECT_TRUE(a1[0].channel == 1 && a1[1].channel == 3);

    auto a2 = run_video_event(bus, ch, fired, Trigger::HBlank,
                              kCyclesPerLine * 6 + kHblankStart);
    EXPECT_EQ(a2.size(), 1u);
    EXPECT_EQ(a2[0].channel, 1);

    // VBlank channels are untouched by HBlank dispatches.
    auto a3 = run_video_event(bus, ch, fired, Trigger::VBlank,
                              kCyclesPerLine * kVblankLine);
    EXPECT_EQ(a3.size(), 0u);
}
