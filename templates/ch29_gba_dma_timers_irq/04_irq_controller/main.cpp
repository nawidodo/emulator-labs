#define LABSTEST_MAIN
#include "labstest.hpp"
#include "irq.hpp"

using namespace gba;

TEST(irq, raise_records_regardless_of_masks) {
    IrqController c;
    c.raise(kIrqHBlank);
    EXPECT_EQ(c.iff & kIrqHBlank, kIrqHBlank);
    EXPECT_FALSE(c.pending());  // not enabled yet

    c.ie = kIrqHBlank;
    c.ime = true;
    EXPECT_TRUE(c.pending());
}

TEST(irq, ime_gates_delivery) {
    IrqController c;
    c.ie = 0xFFFF;
    c.raise(kIrqTimer0);
    EXPECT_FALSE(c.pending());  // IME off
    c.ime = true;
    EXPECT_TRUE(c.pending());
}

TEST(irq, ack_clears_only_requested_bits) {
    IrqController c;
    c.ime = true;
    c.ie = kIrqVBlank | kIrqTimer1;
    c.raise(u16(kIrqVBlank | kIrqTimer1));
    EXPECT_TRUE(c.pending());
    c.acknowledge(kIrqVBlank);
    EXPECT_EQ(c.iff & kIrqVBlank, 0);
    EXPECT_EQ(c.iff & kIrqTimer1, kIrqTimer1);  // still pending
    EXPECT_TRUE(c.pending());
    c.acknowledge(kIrqTimer1);
    EXPECT_FALSE(c.pending());
}

TEST(irq, halt_wake_and_service_order) {
    IrqController c;
    c.ime = true;
    c.raise(kIrqDma3);
    c.ie = kIrqDma2 | kIrqVBlank;  // DMA3 not enabled: no wake
    EXPECT_FALSE(c.should_wake_halt());
    c.ie |= kIrqDma3;
    EXPECT_TRUE(c.should_wake_halt());

    IrqController d;
    d.ime = true;
    d.ie = u16(kIrqVBlank | kIrqTimer3 | kIrqDma1);
    d.raise(u16(kIrqDma1 | kIrqTimer3 | kIrqVBlank));
    EXPECT_EQ(d.next_service_bit(), 0);   // VBlank is bit 0: lowest first
    d.acknowledge(kIrqVBlank);
    EXPECT_EQ(d.next_service_bit(), 6);   // Timer3 bit 6 before DMA1 bit 9
    d.acknowledge(kIrqTimer3);
    EXPECT_EQ(d.next_service_bit(), 9);
    d.acknowledge(kIrqDma1);
    EXPECT_EQ(d.next_service_bit(), -1);
    EXPECT_FALSE(d.should_wake_halt());
}
