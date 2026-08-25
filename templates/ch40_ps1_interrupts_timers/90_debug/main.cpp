#define LABSTEST_MAIN
#include "labstest.hpp"
#include "irq.hpp"

using ps1::sysdev::IrqController;

namespace {
constexpr uint32_t kAll = ps1::sysdev::kAllIrqSources;
}

// REGRESSION: acknowledging one source must not erase other latched lines.
TEST(debug_ack, partial_ack_preserves_other_sources) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqTimer2);
    irq.raise(ps1::sysdev::kIrqController);
    irq.lower(ps1::sysdev::kIrqTimer2 | ps1::sysdev::kIrqController);
    irq.ack(ps1::sysdev::kIrqTimer2);
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqController);
}

// REGRESSION: a source whose line is still asserted re-latches on ack.
TEST(debug_ack, still_asserted_line_relocks) {
    IrqController irq;
    irq.raise(ps1::sysdev::kIrqCdrom);       // device not serviced yet
    irq.ack(ps1::sysdev::kIrqCdrom);
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqCdrom);
    irq.lower(ps1::sysdev::kIrqCdrom);
    irq.ack(ps1::sysdev::kIrqCdrom);
    EXPECT_EQ(irq.status(), 0u);
}

// REGRESSION: ack of a subset keeps the complement bit-exact.
TEST(debug_ack, write_one_clears_only_those_bits) {
    IrqController irq;
    irq.raise(kAll);
    irq.lower(kAll);
    irq.ack(ps1::sysdev::kIrqVblank | ps1::sysdev::kIrqSpu |
            ps1::sysdev::kIrqDma);
    EXPECT_EQ(irq.status(),
              kAll & ~(ps1::sysdev::kIrqVblank | ps1::sysdev::kIrqSpu |
                       ps1::sysdev::kIrqDma));
}

// Sanity: the rest of the controller still behaves while you debug.
TEST(debug_sanity, raise_and_output_still_work) {
    IrqController irq;
    EXPECT_FALSE(irq.irq_out());
    irq.write_mask(ps1::sysdev::kIrqTimer0);
    irq.raise(ps1::sysdev::kIrqTimer0);
    EXPECT_TRUE(irq.irq_out());
    EXPECT_EQ(irq.status(), ps1::sysdev::kIrqTimer0);
}
