#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "legacy_timer.hpp"
#include "timer_events.hpp"

namespace {

struct Op {
    uint64_t cycle;
    bool is_write;
    uint16_t value;  // period for writes / arms
};

// Runs BOTH implementations against the same script and returns whether
// their per-cycle flag states agree everywhere.
bool timelines_match(const std::vector<Op>& ops, uint64_t horizon) {
    legacy::LegacyTimer leg;
    challenge::EventTimer evt;
    leg.arm(ops[0].value);
    evt.arm(0, ops[0].value);

    size_t next_op = 1;  // ops[0] is the arm at cycle 0
    std::vector<bool> mismatches;
    for (uint64_t c = 1; c <= horizon; ++c) {
        if (next_op < ops.size() && ops[next_op].cycle == c) {
            const Op& op = ops[next_op++];
            if (op.is_write) {
                leg.set_period(op.value);
                evt.write_period(op.value, c);
            }
        }
        leg.tick();
        evt.advance_to(c);
        if (leg.flag() != evt.flag()) mismatches.push_back(true);
        leg.clear_flag();
        evt.clear_flag();
    }
    return mismatches.empty();
}

}  // namespace

TEST(equivalence, steady_period) {
    // arm(7) at 0: underflows at 7,14,...49.
    std::vector<Op> ops{{0, false, 7}};
    EXPECT_TRUE(timelines_match(ops, 60));
}

TEST(equivalence, mid_count_period_write) {
    // arm(10); write period 3 at cycle 4 -> first underflow still at 10,
    // then reloads of 3: 13,16,19...
    std::vector<Op> ops{{0, false, 10}, {4, true, 3}};
    EXPECT_TRUE(timelines_match(ops, 40));
}

TEST(equivalence, write_between_underflows) {
    // arm(5), then a fresh period written exactly on an underflow cycle.
    std::vector<Op> ops{{0, false, 5}, {15, true, 8}, {31, true, 2}};
    EXPECT_TRUE(timelines_match(ops, 50));
}

TEST(equivalence, late_service_catch_up) {
    // arm(4) but the event timer is only serviced every 13 cycles — it
    // must apply all missed underflows in order.
    legacy::LegacyTimer leg;
    challenge::EventTimer evt;
    leg.arm(4);
    evt.arm(0, 4);
    uint64_t fires_leg = 0, fires_evt = 0;
    for (uint64_t c = 1; c <= 52; ++c) {
        leg.tick();
        if (leg.flag()) { ++fires_leg; leg.clear_flag(); }
        if (c % 13 == 0) {
            evt.advance_to(c);
            fires_evt = evt.fire_count();
            evt.clear_flag();
        }
    }
    EXPECT_EQ(fires_evt, fires_leg);
    EXPECT_EQ(fires_leg, uint64_t{13});  // underflows at 4,8,...52
}
