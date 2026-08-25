#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_cd.hpp"

#include <vector>

using cdbg::MiniController;

TEST(debug_cd, seek_lands_on_requested_sector) {
    MiniController c;
    int32_t arrived_at = -999;
    uint64_t arrived_t = 0;
    c.set_sink([&](uint64_t t, int32_t lba) {
        arrived_t = t;
        arrived_at = lba;
    });
    c.set_loc_msf(0, 2, 16);   // LBA 16
    c.seek();
    c.tick(1000);
    EXPECT_EQ(arrived_at, 16);  // exactly the requested sector
    EXPECT_EQ(c.current(), 16);
    (void)arrived_t;
}

TEST(debug_cd, completion_respects_seek_latency) {
    MiniController c;
    uint64_t arrived_t = 0;
    bool fired = false;
    c.set_sink([&](uint64_t t, int32_t) {
        arrived_t = t;
        fired = true;
    });
    c.set_loc_msf(0, 3, 0);     // LBA 75 -> latency = 100 + 75 = 175
    c.seek();
    c.tick(174);
    EXPECT_FALSE(fired);        // not there yet — no early interrupt!
    c.tick(1);
    EXPECT_TRUE(fired);
    EXPECT_EQ(arrived_t, 175);
}

TEST(debug_cd, zero_distance_still_costs_base_latency) {
    MiniController c;
    uint64_t arrived_t = 0;
    c.set_sink([&](uint64_t t, int32_t) { arrived_t = t; });
    c.set_loc_msf(0, 2, 0);     // same sector as reset position (LBA 0)
    c.seek();
    c.tick(99);
    EXPECT_EQ(arrived_t, 0u);   // nothing yet
    c.tick(1);
    EXPECT_EQ(arrived_t, 100u); // base cost only
}
