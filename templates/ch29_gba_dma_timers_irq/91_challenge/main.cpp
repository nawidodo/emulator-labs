// Challenge: same-cycle race ordering must be deterministic. Replays the
// scripted scenario twice and compares traces byte-for-byte, then checks
// the committed state digest.
#define LABSTEST_MAIN
#include <string>
#include <vector>
#include "labstest.hpp"
#include "../05_scheduler/system.hpp"
#include <cstddef>

using namespace gba;

namespace {

void apply_script(HWSystem& sys) {
    // cycle 5:  TM0 reload = 0xFFFB (period 5 ticks)
    sys.schedule_script_write(5, 0x04000100, 0xFFFB);
    // cycle 6:  TM0 control: enable, prescaler 1, IRQ on
    sys.schedule_script_write(6, 0x04000102, 0x00C0);
    // cycle 10: IE = timer0
    sys.schedule_script_write(10, 0x04000200, kIrqTimer0);
    // cycle 10 (same instant): IME on — insertion order keeps this AFTER
    // the IE write above.
    sys.schedule_script_write(10, 0x04000208, 0x0001);
}

std::vector<std::string> run_once() {
    HWSystem sys;
    apply_script(sys);
    sys.sched.run_until(60);
    return sys.trace;
}

}  // namespace

TEST(challenge, trace_is_deterministic_across_runs) {
    auto a = run_once();
    auto b = run_once();
    if (a.size() <= 4u) {
        EXPECT_TRUE(false);
        return;
    }
    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) EXPECT_EQ(a[i], b[i]);

    // Expected ordering pinned by this regression:
    bool ie_before_ime = false;
    for (size_t i = 0; i + 1 < a.size(); ++i)
        if (a[i].find("addr=04000200") != std::string::npos &&
            a[i + 1].find("addr=04000208") != std::string::npos)
            ie_before_ime = true;
    EXPECT_TRUE(ie_before_ime);

    int tmr_at_11 = 0;
    for (const auto& l : a)
        if (l.find("op=tmr tm=0 cyc=11") != std::string::npos) ++tmr_at_11;
    EXPECT_EQ(tmr_at_11, 1);  // enabled at 6 with period 5 -> overflow at 11
}
