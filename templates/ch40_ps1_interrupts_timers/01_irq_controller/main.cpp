#define LABSTEST_MAIN
#include "labstest.hpp"
#include "irq.hpp"

using ps1::sysdev::IrqController;

namespace {
constexpr uint32_t kAll = ps1::sysdev::kAllIrqSources;
}

TEST(irq_bits, source_bit_values) {
    EXPECT_EQ(ps1::sysdev::kIrqVblank, 0x001u);
    EXPECT_EQ(ps1::sysdev::kIrqGpu, 0x002u);
    EXPECT_EQ(ps1::sysdev::kIrqCdrom, 0x004u);
    EXPECT_EQ(ps1::sysdev::kIrqDma, 0x008u);
    EXPECT_EQ(ps1::sysdev::kIrqTimer0, 0x010u);
    EXPECT_EQ(ps1::sysdev::kIrqTimer1, 0x020u);
    EXPECT_EQ(ps1::sysdev::kIrqTimer2, 0x040u);
    EXPECT_EQ(ps1::sysdev::kIrqController, 0x080u);
    EXPECT_EQ(ps1::sysdev::kIrqSpu, 0x100u);
    EXPECT_EQ(ps1::sysdev::kIrqPio, 0x200u);
    EXPECT_EQ(ps1::sysdev::kIrqSio, 0x400u);
}

TEST(irq_mask, stores_value_directly) {
    IrqController irq;
    irq.write_mask(0xFFFFu);
    EXPECT_EQ(irq.read_mask(), kAll);
    irq.write_mask(0x1234u);                       // unmapped bits ignored
    EXPECT_EQ(irq.read_mask(), 0x1234u & kAll);
}

TEST(irq_raise, latches_only_requested_bit) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqTimer1);
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqTimer1);
    irq.raise(ps1::sysdev::kIrqCdrom | ps1::sysdev::kIrqDma);
    EXPECT_EQ(irq.status(),
              ps1::sysdev::kIrqTimer1 | ps1::sysdev::kIrqCdrom |
                  ps1::sysdev::kIrqDma);
}

TEST(irq_raise, reassert_keeps_latch_set) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqVblank);
    irq.raise(ps1::sysdev::kIrqVblank);            // vblank fires every frame
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqVblank);
}

TEST(irq_ack, write_one_clears_only_those_bits) {
    IrqController irq;
    irq.raise(kAll);
    irq.lower(kAll);                               // all lines idle now
    irq.ack(ps1::sysdev::kIrqTimer0 | ps1::sysdev::kIrqSpu);
    EXPECT_EQ(irq.status(), kAll & ~(ps1::sysdev::kIrqTimer0 | ps1::sysdev::kIrqSpu));
    irq.ack(0);                                    // no-op
    EXPECT_EQ(irq.status(), kAll & ~(ps1::sysdev::kIrqTimer0 | ps1::sysdev::kIrqSpu));
    irq.ack(kAll);
    EXPECT_EQ(irq.status(), 0u);
}

TEST(irq_ack, still_asserted_line_relocks) {
    IrqController irq;
    // CDROM raises; software acknowledges BEFORE servicing the device, so
    // the raw line is still held and must re-latch.
    irq.raise(ps1::sysdev::kIrqCdrom);
    irq.ack(ps1::sysdev::kIrqCdrom);
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqCdrom);
    // Servicing the device drops the line; only then does the ack stick.
    irq.lower(ps1::sysdev::kIrqCdrom);
    irq.ack(ps1::sysdev::kIrqCdrom);
    EXPECT_EQ(irq.status(), 0u);
}

TEST(irq_ack, partial_ack_does_not_disturb_other_sources) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqTimer2);
    irq.raise(ps1::sysdev::kIrqController);
    irq.lower(ps1::sysdev::kIrqTimer2 | ps1::sysdev::kIrqController);
    irq.ack(ps1::sysdev::kIrqTimer2);
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqController);
    // Periodic timer refires on its next period even after being acked.
    irq.raise(ps1::sysdev::kIrqTimer2);
    EXPECT_EQ(irq.status(),
              ps1::sysdev::kIrqTimer2 | ps1::sysdev::kIrqController);
}

TEST(irq_out, gated_by_mask) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqTimer0);
    EXPECT_FALSE(irq.irq_out());                   // masked off
    irq.write_mask(ps1::sysdev::kIrqTimer0);
    EXPECT_TRUE(irq.irq_out());
    irq.ack(ps1::sysdev::kIrqTimer0);              // line lowered first
    irq.lower(ps1::sysdev::kIrqTimer0);
    irq.ack(ps1::sysdev::kIrqTimer0);
    EXPECT_FALSE(irq.irq_out());
}

TEST(irq_out, any_enabled_source_suffices) {
    IrqController irq;
    irq.write_mask(ps1::sysdev::kIrqVblank | ps1::sysdev::kIrqSpu);
    irq.raise(ps1::sysdev::kIrqSpu);
    EXPECT_TRUE(irq.irq_out());
    irq.lower(ps1::sysdev::kIrqSpu);
    irq.ack(ps1::sysdev::kIrqSpu);
    EXPECT_FALSE(irq.irq_out());                   // vblank enabled but quiet
}
