#define LABSTEST_MAIN
#include "labstest.hpp"
#include "../02_mini_devices/system.hpp"
#include "debug_scheduler.hpp"

#include <string>
#include <vector>

using ps1dbg::Scheduler;

namespace {

std::vector<std::string> lines_at(const std::vector<std::string>& log,
                                  const std::string& cyc_prefix) {
    std::vector<std::string> out;
    for (const auto& line : log)
        if (line.rfind(cyc_prefix, 0) == 0) out.push_back(line);
    return out;
}

}  // namespace

// Scenario: the CD read is kicked at cycle 0, so its completion deadline
// (0 + kCdSectorCycles = 19968) is inserted into the queue long before the
// SPU sample chain schedules tick #26 (scheduled at cycle 19200 for
// deadline 19968). Same timestamp, different insertion times: FIFO says
// the CD completion dispatches first and its IRQ2 latch precedes the SPU
// sample latch. This is the deterministic latch-order guarantee games
// rely on when two interrupts fire in one dispatch batch.
TEST(debug, cd_irq_precedes_spu_irq_on_shared_deadline) {
    Scheduler sch;
    ps1sys::System<Scheduler> sys(sch);
    sys.set_cpu_enabled(false);
    sys.reset();
    sys.store32(ps1sys::kSpuCtrl, 1);  // sample-period IRQ on (line 9)
    sys.store32(ps1sys::kCdCmd, 1);    // CD read -> done at 19968

    sys.run_until(20000);

    const auto batch = lines_at(sys.event_log(), "cyc=19968 ");
    EXPECT_EQ(batch.size(), size_t(3));
    if (batch.size() == 3) {
        EXPECT_EQ(batch[0], std::string("cyc=19968 evt=cd_done lba=1"));
        EXPECT_EQ(batch[1],
                  std::string("cyc=19968 evt=latch line=2 src=cd"));
        EXPECT_EQ(batch[2],
                  std::string("cyc=19968 evt=latch line=9 src=spu"));
    }
}

// Same guarantee, pinned directly against the scheduler primitive.
TEST(debug, equal_timestamps_dispatch_in_insertion_order) {
    Scheduler sch;
    std::vector<std::string> fired;
    // Inserted early (like a latency deadline): must come out first even
    // though the recurring chain below keeps inserting behind it.
    sch.schedule(100, [&] { fired.emplace_back("cd"); }, "cd");
    for (int i = 1; i <= 5; ++i)
        sch.schedule(20 * i, [&] { fired.emplace_back("tick"); }, "tick");
    // Tick #5 lands exactly on the CD deadline (seq is higher).
    sch.run_until(100);
    EXPECT_EQ(fired.size(), size_t(6));
    if (fired.size() != 6) return;
    EXPECT_EQ(fired[0], std::string("tick"));
    EXPECT_EQ(fired[3], std::string("tick"));  // ticks at 20..80 first
    EXPECT_EQ(fired[4], std::string("cd"));    // FIFO at the shared cycle
    EXPECT_EQ(fired[5], std::string("tick"));
}
