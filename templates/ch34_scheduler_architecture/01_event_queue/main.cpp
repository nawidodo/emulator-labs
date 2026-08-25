#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <string>
#include <vector>

#include "scheduler.hpp"

namespace {

// Records dispatch order as "name@ts" so tests can assert exact order.
struct Recorder {
    std::vector<std::string> log;
    sched::Scheduler sched;

    void add(uint64_t ts, const char* name) {
        std::string n = name;
        sched.schedule(ts, [this, n, ts] { log.push_back(n + "@" + std::to_string(ts)); },
                       name);
    }
};

}  // namespace

TEST(sched, fires_in_timestamp_order) {
    Recorder r;
    r.add(30, "late");
    r.add(10, "early");
    r.add(20, "mid");
    r.sched.run_until(1000);
    EXPECT_EQ(r.log.size(), size_t{3});
    EXPECT_EQ(r.log[0], "early@10");
    EXPECT_EQ(r.log[1], "mid@20");
    EXPECT_EQ(r.log[2], "late@30");
}

TEST(sched, equal_timestamps_fifo) {
    // Same timestamp must dispatch in insertion order — hardware never
    // reorders its own same-instant events.
    Recorder r;
    r.add(5, "first");
    r.add(5, "second");
    r.add(5, "third");
    r.add(4, "before");
    r.sched.run_until(10);
    EXPECT_EQ(r.log.size(), size_t{4});
    EXPECT_EQ(r.log[0], "before@4");
    EXPECT_EQ(r.log[1], "first@5");
    EXPECT_EQ(r.log[2], "second@5");
    EXPECT_EQ(r.log[3], "third@5");
}

TEST(sched, run_until_boundary_is_inclusive) {
    Recorder r;
    r.add(9, "inside");
    r.add(10, "boundary");
    r.add(11, "outside");
    r.sched.run_until(10);  // limit itself is a valid deadline
    EXPECT_EQ(r.log.size(), size_t{2});
    EXPECT_EQ(r.log[1], "boundary@10");
    EXPECT_EQ(r.sched.now(), uint64_t{10});
    EXPECT_EQ(r.sched.pending(), size_t{1});
}

TEST(sched, step_dispatches_exactly_one) {
    Recorder r;
    r.add(1, "a");
    r.add(2, "b");
    EXPECT_TRUE(r.sched.step());
    EXPECT_EQ(r.log.size(), size_t{1});
    EXPECT_EQ(r.log[0], "a@1");
    EXPECT_EQ(r.sched.now(), uint64_t{1});
}

TEST(sched, handler_may_schedule_reentrantly) {
    sched::Scheduler s;
    std::vector<std::string> log;
    s.schedule(4, [&] {
        log.push_back("a@4");
        // Reentrant: same timestamp and later, both legal.
        s.schedule(4, [&] { log.push_back("b@4"); }, "b");
        s.schedule(7, [&] { log.push_back("c@7"); }, "c");
    }, "a");
    s.run_until(6);
    EXPECT_EQ(log.size(), size_t{2});
    EXPECT_EQ(log[0], "a@4");
    EXPECT_EQ(log[1], "b@4");  // FIFO tie-break holds for reentrant adds
    EXPECT_EQ(s.pending(), size_t{1});
    s.run_until(100);
    EXPECT_EQ(log.size(), size_t{3});
    EXPECT_EQ(s.now(), uint64_t{7});
}

TEST(sched, empty_queue_is_quiet) {
    sched::Scheduler s;
    EXPECT_FALSE(s.step());
    s.run_until(12345);  // no-op, no crash
    EXPECT_EQ(s.now(), uint64_t{0});
}
