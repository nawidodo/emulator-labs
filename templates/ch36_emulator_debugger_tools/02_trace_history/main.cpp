#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "trace.hpp"

TEST(history, ring_wraps_and_keeps_newest) {
    tools::HistoryRing ring(4);
    for (uint8_t i = 0; i < 6; ++i) {
        tools::TraceRecord r;
        r.pc = i;
        r.op = 0x01;
        ring.push(r);
    }
    EXPECT_EQ(ring.size(), size_t{4});
    // Newest is pc=5; oldest kept is pc=2 (2 entries were evicted).
    EXPECT_EQ(ring.at(0)->pc, uint8_t{5});
    EXPECT_EQ(ring.at(1)->pc, uint8_t{4});
    EXPECT_EQ(ring.at(3)->pc, uint8_t{2});
    EXPECT_TRUE(ring.at(4) == nullptr);
}

TEST(history, format_is_canonical) {
    tools::TraceRecord r;
    r.pc = 0x04;
    r.op = 0x04;
    r.a = 0x05;
    r.cyc = 11;
    EXPECT_EQ(tools::format(r), "pc=04 op=04 a=05 cyc=11");
}

TEST(trace, op_range_filter_inclusive) {
    // Trace arithmetic ops (0x04..0x06) only.
    tools::TraceLogger::Filter f;
    f.by_op_range = true;
    f.op_lo = 0x04;
    f.op_hi = 0x06;
    tools::TraceLogger log(f);

    const std::vector<tools::TraceRecord> recs{
        {0x00, 0x01, 5, 0},   // LDA — filtered out
        {0x02, 0x04, 7, 2},   // ADD — kept
        {0x04, 0xFF, 7, 4},   // HALT — filtered out
        {0x05, 0x06, 1, 5},   // SUB — kept (hi bound inclusive)
    };
    for (const auto& r : recs) log.log(r);
    EXPECT_EQ(log.seen(), uint64_t{4});
    EXPECT_EQ(log.kept().size(), size_t{2});
    if (log.kept().size() == 2) {
        EXPECT_EQ(log.kept()[0].op, uint8_t{0x04});
        EXPECT_EQ(log.kept()[1].op, uint8_t{0x06});
    }
}

TEST(trace, pc_filter_matches_executed_pc) {
    tools::TraceLogger::Filter f;
    f.by_pc = true;
    f.pc = 0x02;
    tools::TraceLogger log(f);
    const std::vector<tools::TraceRecord> recs{
        {0x02, 0x04, 3, 0},
        {0x03, 0x07, 3, 2},
        {0x02, 0x04, 4, 4},
    };
    for (const auto& r : recs) log.log(r);
    EXPECT_EQ(log.kept().size(), size_t{2});
}

TEST(trace, combined_filters_and_together) {
    tools::TraceLogger::Filter f;
    f.by_op_range = true;
    f.op_lo = 0x03;
    f.op_hi = 0x05;
    f.by_pc = true;
    f.pc = 0x10;
    tools::TraceLogger log(f);
    const std::vector<tools::TraceRecord> recs{
        {0x10, 0x04, 9, 0},  // matches both
        {0x11, 0x04, 9, 2},  // right op, wrong pc
        {0x10, 0x07, 9, 4},  // right pc, wrong op
    };
    for (const auto& r : recs) log.log(r);
    EXPECT_EQ(log.kept().size(), size_t{1});
}
