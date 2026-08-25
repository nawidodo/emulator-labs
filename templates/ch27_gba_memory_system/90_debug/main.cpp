#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_bus.hpp"

using namespace debugbus;

TEST(debug_suite, bug1_refill_forces_nonsequential_cost) {
    DebugBus bus;
    bus.note_access(0x020000FE, 2);
    bus.notify_refill();                       // a branch just refilled
    bus.note_access(0x02000100, 2);            // adjacent but still N
    const BusResult r = bus.read(0x02000100, 2);
    EXPECT_EQ(r.cycles, 3u);                   // EWRAM non-sequential
}

TEST(debug_suite, bug2_ewram_mirrors_every_256k) {
    DebugBus bus;
    bus.write(0x02000010, 4, 0x12345678u);
    EXPECT_EQ(bus.read(0x02040010, 2).value, 0x5678u);  // +256K mirror
    // 64K fold would wrongly alias this onto offset 0x10 of block 0.
    EXPECT_EQ(bus.read(0x02010010, 4).value,
              bus.read(0x02010000, 4).value);
}

TEST(debug_suite, bug3_vram_obj_bank_is_not_bg) {
    DebugBus bus;
    bus.write(0x06004444, 2, 0x1111);
    bus.write(0x06014444, 2, 0x2222);
    EXPECT_EQ(bus.read(0x06004444, 2).value, 0x1111u);
    EXPECT_EQ(bus.read(0x06014444, 2).value, 0x2222u);
    EXPECT_EQ(bus.read(0x06018888, 2).value,
              bus.read(0x06008888, 2).value);  // top hole DOES mirror BG
}

TEST(debug_suite, bug4_sram_reads_answer_one_byte) {
    DebugBus bus;
    bus.sram[0x10] = 0xAB;
    bus.sram[0x11] = 0xCD;
    const BusResult r = bus.read(0x0E000010, 2);
    EXPECT_EQ(r.value, 0xABu);                 // no phantom second byte
}

TEST(debug_suite, bug5_open_bus_latches_last_value) {
    DebugBus bus;
    bus.iwram[0x30] = 0xEF;
    const BusResult mapped = bus.read(0x03000030, 1);
    EXPECT_EQ(mapped.value, 0xEFu);
    const BusResult unmapped = bus.read(0x10000000, 4);
    EXPECT_EQ(unmapped.value, 0xEFu);          // last driven value, not 0
}

TEST(hidden, debug_hidden_bus_invariants) {
    DebugBus bus;
    bus.write(0x020000A0, 4, 0x0A0B0C0Du);
    EXPECT_EQ(bus.read(0x020100A0, 4).value, 0u);   // different EWRAM block
    EXPECT_EQ(bus.read(0x024000A0, 2).value & 0xFFFFu, 0x0C0Du); // mirror
}
