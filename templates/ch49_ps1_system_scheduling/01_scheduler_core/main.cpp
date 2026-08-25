#define LABSTEST_MAIN
#include "labstest.hpp"
#include "scheduler.hpp"

#include <string>
#include <vector>

using sched::Scheduler;

// Collect dispatched event names in dispatch order.
static std::vector<std::string> fired;

static void arm(Scheduler& s, uint64_t ts, const std::string& name) {
    s.schedule(ts, [name] { fired.push_back(name); }, name.c_str());
}

TEST(sched, orders_by_timestamp) {
    fired.clear();
    Scheduler s;
    arm(s, 30, "late");
    arm(s, 10, "first");
    arm(s, 20, "second");
    s.run_until(100);
    EXPECT_EQ(fired.size(), size_t(3));
    if (fired.size() == 3) {
        EXPECT_TRUE(fired[0] == "first");
        EXPECT_TRUE(fired[1] == "second");
        EXPECT_TRUE(fired[2] == "late");
    }
    EXPECT_EQ(s.now(), uint64_t(30));
}

TEST(sched, equal_timestamps_dispatch_fifo) {
    fired.clear();
    Scheduler s;
    // Three events at the SAME instant: hardware latches request lines in
    // arrival order; the seq tie-break must reproduce that exactly.
    arm(s, 50, "a");
    arm(s, 50, "b");
    arm(s, 50, "c");
    s.run_until(50);
    EXPECT_EQ(fired.size(), size_t(3));
    if (fired.size() == 3) {
        EXPECT_TRUE(fired[0] == "a");
        EXPECT_TRUE(fired[1] == "b");
        EXPECT_TRUE(fired[2] == "c");
    }
}

TEST(sched, run_until_respects_deadline) {
    fired.clear();
    Scheduler s;
    arm(s, 10, "at_deadline");
    arm(s, 11, "past_deadline");
    s.run_until(10);  // inclusive: an event exactly AT the deadline fires
    EXPECT_EQ(fired.size(), size_t(1));
    if (fired.size() == 1) EXPECT_TRUE(fired[0] == "at_deadline");
    EXPECT_EQ(s.pending(), size_t(1));
    EXPECT_EQ(s.now(), uint64_t(10));
}

TEST(sched, events_scheduled_during_dispatch_meet_deadline) {
    fired.clear();
    Scheduler s;
    s.schedule(5, [&s] {
        s.schedule(7, [] { fired.push_back("chained"); }, "chained");
    }, "seed");
    arm(s, 6, "between");
    s.run_until(100);
    EXPECT_EQ(fired.size(), size_t(2));
    if (fired.size() == 2) {
        EXPECT_TRUE(fired[0] == "between");
        EXPECT_TRUE(fired[1] == "chained");
    }
}

TEST(sched, cancel_prevents_dispatch) {
    fired.clear();
    Scheduler s;
    uint64_t id = s.schedule(20, [] { fired.push_back("ghost"); }, "ghost");
    arm(s, 25, "keeper");
    s.cancel(id);
    s.run_until(100);
    EXPECT_EQ(fired.size(), size_t(1));
    if (fired.size() == 1) EXPECT_TRUE(fired[0] == "keeper");
    EXPECT_FALSE(s.step());  // only the cancelled husk remained
}

TEST(sched, reentrant_same_timestamp_goes_after_current) {
    fired.clear();
    Scheduler s;
    s.schedule(40, [&s] {
        fired.push_back("outer");
        // Re-scheduling at your OWN timestamp must not re-enter before the
        // current handler returns: it queues behind in insertion order.
        s.schedule(40, [] { fired.push_back("inner"); }, "inner");
    }, "outer");
    s.run_until(40);
    EXPECT_EQ(fired.size(), size_t(2));
    if (fired.size() == 2) {
        EXPECT_TRUE(fired[0] == "outer");
        EXPECT_TRUE(fired[1] == "inner");
    }
    EXPECT_EQ(s.now(), uint64_t(40));
}
