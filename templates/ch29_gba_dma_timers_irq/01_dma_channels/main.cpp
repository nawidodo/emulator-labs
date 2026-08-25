#define LABSTEST_MAIN
#include "labstest.hpp"
#include "dma.hpp"

using namespace gba;

TEST(addrstep, per_mode_and_width) {
    EXPECT_EQ(addr_step(0, false), 2);
    EXPECT_EQ(addr_step(0, true), 4);
    EXPECT_EQ(addr_step(1, true), 4);   // inc+reload acts like inc in burst
    EXPECT_EQ(addr_step(2, false), -2);
    EXPECT_EQ(addr_step(2, true), -4);
    EXPECT_EQ(addr_step(3, true), 0);   // fixed (FIFO / EEPROM style)
}

TEST(dma, forward_halfword_copy) {
    Bus bus;
    for (int i = 0; i < 8; ++i) bus.wr16(0x02000000 + u32(i) * 2, u16(0x100 + i));
    DmaRegs r;
    r.sad = 0x02000000;
    r.dad = 0x03000000;
    r.count = 8;
    r.control = 0x8000;  // enable, everything else default: 16-bit inc/inc
    bool irq = false;
    u64 cyc = run_immediate_transfer(bus, r, irq);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(bus.rd16(0x03000000 + u32(i) * 2), u16(0x100 + i));
    EXPECT_EQ(cyc, 48u);  // 8 units * 6 cycles
}

TEST(dma, word_copy_and_fixed_src_fifo_fill) {
    Bus bus;
    bus.wr32(0x02000010, 0xDEADBEEF);
    // Fixed source, incrementing dst, 32-bit, count 3.
    DmaRegs r;
    r.sad = 0x02000010;
    r.dad = 0x02000100;
    r.count = 3;
    r.control = u16(0x8000 | 1 << 10 | 3 << 7);
    bool irq = false;
    run_immediate_transfer(bus, r, irq);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(bus.rd32(0x02000100 + u32(i) * 4), 0xDEADBEEFu);
    // Decrementing destination (stack-fill pattern): units land at the
    // start address and below it.
    bus.wr16(0x02000210, 0x4242);
    DmaRegs s;
    s.sad = 0x02000210;
    s.dad = 0x02000300;
    s.count = 2;
    s.control = u16(0x8000 | 2 << 5 | 3 << 7);  // dec dst, fixed src
    run_immediate_transfer(bus, s, irq);
    EXPECT_EQ(bus.rd16(0x02000300), 0x4242);   // first unit at start addr
    EXPECT_EQ(bus.rd16(0x020002FE), 0x4242);   // then stepping down
    EXPECT_EQ(bus.rd16(0x020002FC), 0x0000);   // untouched
}

TEST(dma, enable_clears_and_irq_flags) {
    Bus bus;
    DmaRegs r;
    r.sad = 0x02000000;
    r.dad = 0x02000080;
    r.count = 1;
    r.control = u16(0x8000 | 1 << 14);  // enable + irq on complete
    bool irq = false;
    run_immediate_transfer(bus, r, irq);
    EXPECT_TRUE(irq);
    EXPECT_FALSE(r.enable());  // auto-cleared

    bool irq2 = true;
    run_immediate_transfer(bus, r, irq2);  // disabled now: no-op
    EXPECT_FALSE(irq2);

    DmaRegs q;
    q.sad = 0x02000000;
    q.dad = 0x02000090;
    q.count = 1;
    q.control = 0x8000;  // no irq bit
    bool irq3 = false;
    run_immediate_transfer(bus, q, irq3);
    EXPECT_FALSE(irq3);
}

TEST(dma, count_wrap_full_range) {
    Bus bus;
    DmaRegs r;
    r.sad = 0x02000000;
    r.dad = 0x02001000;
    r.count = 0;          // full range
    r.control = 0x8000;
    bool irq = false;
    u64 cyc = run_immediate_transfer(bus, r, irq);
    EXPECT_EQ(cyc, 6u * 0x10000u);
}
