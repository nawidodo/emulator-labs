#define LABSTEST_MAIN
#include "labstest.hpp"
#include "controller.hpp"

#include <vector>

using namespace cdrom;

namespace {
// Drain all visible responses for the current interrupt.
std::vector<uint8_t> drain(CdRomController& c) {
    std::vector<uint8_t> out;
    while (c.resp_available()) out.push_back(c.read_response());
    return out;
}
}  // namespace

TEST(ctrl, getstat_answers_int3_immediately) {
    CdRomController c;
    c.issue(kCmdGetStat);
    EXPECT_EQ(c.irq_level(), 3);
    EXPECT_TRUE(drain(c).size() == 1);  // single STAT byte
    c.ack_irq();
    EXPECT_EQ(c.irq_level(), 0);
}

TEST(ctrl, setloc_decodes_bcd_params_to_lba) {
    CdRomController c;
    const int32_t want = msf_to_lba(0, 2, 16);
    // BCD 00 02 16.
    c.write_param(0x00);
    c.write_param(0x02);
    c.write_param(0x16);
    c.issue(kCmdSetloc);
    EXPECT_EQ(c.irq_level(), 3);
    EXPECT_EQ(c.target_lba(), want);
    c.ack_irq();
}

TEST(ctrl, init_two_phase_with_spinup_latency) {
    CdRomController c;
    c.issue(kCmdInit);
    EXPECT_EQ(c.irq_level(), 3);
    EXPECT_NE(c.stat() & kStatMotorOn, 0);
    c.ack_irq();
    EXPECT_EQ(c.irq_level(), 0);       // second phase NOT due yet

    c.tick(600);
    EXPECT_EQ(c.irq_level(), 0);
    c.tick(599);
    EXPECT_EQ(c.irq_level(), 0);       // 1199 < 1200: still pending
    c.tick(1);
    EXPECT_EQ(c.irq_level(), 2);       // exactly at 1200
    EXPECT_TRUE(drain(c).size() == 1);
    c.ack_irq();
    EXPECT_EQ(c.irq_level(), 0);
}

TEST(ctrl, pause_stops_reading_and_completes_late) {
    CdRomController c;
    c.set_stat_bits(kStatRead | kStatSeek);
    c.set_reading(true);

    c.issue(kCmdPause);
    EXPECT_EQ(c.irq_level(), 3);
    EXPECT_EQ(c.stat() & (kStatRead | kStatSeek), 0);
    EXPECT_FALSE(c.reading());          // streaming stopped immediately
    c.ack_irq();

    c.tick(249);
    EXPECT_EQ(c.irq_level(), 0);
    c.tick(1);
    EXPECT_EQ(c.irq_level(), 2);
}

TEST(ctrl, unknown_command_errors_with_int5) {
    CdRomController c;
    c.issue(0x7F);
    EXPECT_EQ(c.irq_level(), 5);
    const auto resp = drain(c);
    EXPECT_TRUE(resp.size() == 1);
    EXPECT_NE(resp[0] & kStatError, 0);
}

TEST(ctrl, irq_queue_preserves_response_order) {
    CdRomController c;
    c.issue(kCmdInit);                  // queues INT3 now, INT2 later
    EXPECT_EQ(c.irq_level(), 3);
    c.tick(2000);                       // second phase fires while INT3 unacked
    EXPECT_EQ(c.irq_level(), 3);        // still front of queue
    c.ack_irq();                        // reveal INT2
    EXPECT_EQ(c.irq_level(), 2);
    EXPECT_TRUE(drain(c).size() == 1);
    c.ack_irq();
    EXPECT_EQ(c.irq_level(), 0);
}
