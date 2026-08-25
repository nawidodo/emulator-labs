#define LABSTEST_MAIN
#include "labstest.hpp"
#include "../01_scheduler_core/scheduler.hpp"
#include "../02_mini_devices/system.hpp"
#include "../shared/fnv.hpp"
#include "fixture.hpp"
#include "golden.hpp"
#include <cstddef>

#include <string>
#include <vector>

using namespace ps1sys;

namespace {

struct RunOut {
    Log event_log;
    std::vector<std::string> trace;
    bool halted = false;
};

RunOut run_boot() {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    const auto rom = boot_handshake_rom();
    sys.load_rom(reinterpret_cast<const uint8_t*>(rom.data()),
                 rom.size() * 4);
    sys.run_until(40000);
    RunOut out;
    out.event_log = sys.event_log();
    out.trace = sys.trace();
    out.halted = sys.halted();
    return out;
}

uint64_t log_hash(const Log& log) {
    std::string blob;
    for (const auto& l : log) {
        blob += l;
        blob += '\n';
    }
    return labshash::fnv64(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(blob.data()), blob.size()));
}

}  // namespace

TEST(challenge, event_log_matches_public_golden) {
    const auto r = run_boot();
    EXPECT_TRUE(r.halted);
    EXPECT_EQ(log_hash(r.event_log), kGoldenBootHandshakeFnv64);
}

TEST(challenge, handshake_reaches_all_milestones_in_order) {
    const auto r = run_boot();
    std::vector<std::string> milestones;
    for (const auto& line : r.event_log)
        if (line.find("evt=milestone") != std::string::npos)
            milestones.push_back(line);
    EXPECT_EQ(milestones.size(), size_t(4));
    if (milestones.size() != 4) return;
    EXPECT_EQ(milestones[0], std::string("cyc=24 evt=milestone val=1"));
    EXPECT_EQ(milestones[1], std::string("cyc=20032 evt=milestone val=2"));
    EXPECT_EQ(milestones[2], std::string("cyc=20056 evt=milestone val=3"));
    EXPECT_EQ(milestones[3], std::string("cyc=20068 evt=milestone val=7"));
    EXPECT_TRUE(r.halted);
}

TEST(challenge, cd_irq_latches_before_spu_sample_at_shared_deadline) {
    // The CD deadline (20000) does not collide with an SPU boundary in
    // this fixture, but the ordering guarantee is what the hash pins:
    // every latch appears at its deterministic cycle in dispatch order.
    const auto r = run_boot();
    size_t gpu_latch = 0, cd_latch = 0, spu_latches = 0;
    for (const auto& line : r.event_log) {
        if (line.find("latch line=1 src=gpu") != std::string::npos)
            ++gpu_latch;
        if (line.find("latch line=2 src=cd") != std::string::npos)
            ++cd_latch;
        if (line.find("latch line=9 src=spu") != std::string::npos)
            ++spu_latches;
    }
    // GPU idle IRQ once; CD completion once; SPU sample boundaries 1..26.
    EXPECT_EQ(gpu_latch, size_t(1));
    EXPECT_EQ(cd_latch, size_t(1));
    EXPECT_EQ(spu_latches, size_t(26));
}
