// ch34 coding test: synchronize devices with periods 3, 5, 7 on one
// integer master clock; emit the exact event-order log.
//
//   ch34_99_sync3 --cycles N --out FILE [--help]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <queue>
#include <string>
#include <vector>

namespace sync3 {

struct Event {
    uint64_t timestamp;
    uint64_t seq;
    int dev;  // 0=a 1=b 2=c

//@LABS-BEGIN 1
//@LABS-SOLUTION
    bool operator>(const Event& rhs) const {
        if (timestamp != rhs.timestamp) return timestamp > rhs.timestamp;
        return seq > rhs.seq;  // FIFO tie-break in scheduling order
    }
//@LABS-STUB
    // TODO(1): earliest timestamp first; equal timestamps in insertion
    // (seq) order.
    bool operator>(const Event&) const { return false; }  // wrong on purpose
//@LABS-END
};

// Runs all three devices to `limit` inclusive, appending one log line per
// dispatch. Returns the number of events dispatched.
inline uint64_t run(uint64_t limit, std::vector<std::string>& log) {
    const uint64_t period[3] = {3, 5, 7};
    const char* name[3] = {"a", "b", "c"};
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> q;
    uint64_t seq = 0;

    // Schedule each device's first deadline in a,b,c order.
    for (int d = 2; d >= 0; --d) {
        q.push(Event{period[d], seq++, d});
    }

//@LABS-BEGIN 2
//@LABS-SOLUTION
    uint64_t fired = 0;
    while (!q.empty() && q.top().timestamp <= limit) {
        Event ev = q.top();
        q.pop();
        log.push_back("cyc=" + std::to_string(ev.timestamp) + " dev=" +
                      name[ev.dev]);
        ++fired;
        q.push(Event{ev.timestamp + period[ev.dev], seq++, ev.dev});
    }
    return fired;
//@LABS-STUB
    // TODO(2): dispatch loop — pop while the earliest deadline is <=
    // limit, append "cyc=<ts> dev=<name>" to `log`, count it, and push
    // that device's NEXT deadline (+ its period). Return the count.
    (void)name;
    (void)period;
    (void)seq;
    (void)q;
    return 0;  // wrong on purpose: nothing runs
//@LABS-END
}

}  // namespace sync3

int main(int argc, char** argv) {
    uint64_t cycles = 100;
    const char* out_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) {
            std::printf(
                "usage: ch34_99_sync3 --cycles N --out FILE\n"
                "\nSynchronizes devices with periods 3/5/7 and writes the\n"
                "exact event-order log.\n");
            return 0;
        }
        if (!std::strcmp(argv[i], "--cycles") && i + 1 < argc) {
            cycles = std::strtoull(argv[++i], nullptr, 0);
        } else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) {
            out_path = argv[++i];
        }
    }
    if (!out_path) {
        std::fprintf(stderr, "error: --out FILE required (--help)\n");
        return 2;
    }

    std::vector<std::string> log;
    const uint64_t n = sync3::run(cycles, log);
    if (n == 0) {
        std::fprintf(stderr, "error: scheduler produced no events\n");
        return 1;
    }
    std::ofstream out(out_path, std::ios::binary);
    for (const auto& l : log) out << l << "\n";
    std::printf("events=%llu\n",
                static_cast<unsigned long long>(n));
    return 0;
}
