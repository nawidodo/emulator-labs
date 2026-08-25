#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>
#include <cstdio>

#include <string>
#include <vector>

#include "scheduler.hpp"

namespace {
struct Recorder {
    std::vector<std::string> log;
    sched::Scheduler sched;
    void add(uint64_t ts, const char* name) {
        std::string n = name;
        sched.schedule(ts,
                       [this, n, ts] { log.push_back(n + "@" + std::to_string(ts)); },
                       name);
    }
};
}  // namespace

TEST(debug_sched, ordering_ok) {
    Recorder r;
    r.add(30, "late");
    r.add(10, "early");
    r.sched.run_until(100);
    EXPECT_EQ(r.log[0], "early@10");
}

TEST(debug_sched, equal_timestamps_fifo) {
    // RED with the seeded bug: heap layout decides tie order.
    Recorder r;
    char name[8];
    for (int i = 0; i < 8; ++i) {
        std::snprintf(name, sizeof(name), "e%d", i);
        r.add(5, name);
    }
    r.add(4, "before");
    r.sched.run_until(10);
    EXPECT_EQ(r.log.size(), size_t{9});
    for (int i = 0; i < 8; ++i) {
        char want[8];
        std::snprintf(want, sizeof(want), "e%d@5", i);
        EXPECT_EQ(r.log[size_t(i) + 1], std::string(want));
    }
}
