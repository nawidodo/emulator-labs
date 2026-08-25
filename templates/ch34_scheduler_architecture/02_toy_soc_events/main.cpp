#define LABSTEST_MAIN
#include "labstest.hpp"

#include <array>
#include <cstdint>
#include <string>

#include "soc.hpp"

TEST(soc, boot_schedules_cpu_and_first_timer_tick) {
    std::array<uint8_t, 4> prog{0xFF, 0, 0, 0};  // HALT immediately
    soc::SoC s(prog);
    s.boot();
    EXPECT_EQ(s.pending_events(), size_t{2});  // cpu@0 + timer@10

    s.run_until(0);  // dispatch only the cycle-0 events
    EXPECT_TRUE(s.halted());
    EXPECT_EQ(s.timer.fire_count, uint64_t{0});  // first tick is at cyc=10
}

TEST(soc, timer_fires_every_period_on_master_clock) {
    std::array<uint8_t, 1> prog{0xFF};
    soc::SoC s(prog);
    s.boot();
    s.run_until(99);  // deadlines 10,20,...,90 -> 9 fires
    EXPECT_EQ(s.timer.fire_count, uint64_t{9});

    uint64_t n = 0;
    std::string first_timer_line;
    for (const auto& line : s.event_log) {
        if (line.find("dev=timer") != std::string::npos) {
            if (first_timer_line.empty()) first_timer_line = line;
            ++n;
        }
    }
    EXPECT_EQ(n, uint64_t{9});
    EXPECT_EQ(first_timer_line, "cyc=10 dev=timer");
}

TEST(soc, uart_transmits_one_byte_per_period) {
    // LDA #'A'; OUT; LDA #'B'; OUT; HALT
    std::array<uint8_t, 7> prog{0x01, 'A', 0x0B, 0x01, 'B', 0x0B, 0xFF};
    soc::SoC s(prog);
    s.boot();
    s.run_until(1000);
    EXPECT_EQ(s.uart.transmitted.size(), size_t{2});
    EXPECT_EQ(s.uart.transmitted[0], uint8_t{'A'});
    EXPECT_EQ(s.uart.transmitted[1], uint8_t{'B'});
    // OUT of 'A' starts at cyc=2 -> byte completes at 2+8=10; 'B' queued
    // back-to-back finishes at 18.
    bool saw10 = false, saw18 = false;
    for (const auto& line : s.event_log) {
        if (line == "cyc=10 dev=uart") saw10 = true;
        if (line == "cyc=18 dev=uart") saw18 = true;
    }
    EXPECT_TRUE(saw10);
    EXPECT_TRUE(saw18);
}

TEST(soc, event_interleaving_is_exact) {
    // addr 00: 04 01   ADD #1      (2cy, executed once)
    // addr 02: 03 20   STA $20     (3cy)
    // addr 04: 07 02   JMP $02     (2cy)  -> loop = 5 cycles
    std::array<uint8_t, 6> prog{0x04, 0x01, 0x03, 0x20, 0x07, 0x02};
    soc::SoC s(prog);
    s.boot();
    s.run_until(50);

    std::string joined;
    for (const auto& l : s.event_log) joined += l + ";";
    // CPU instruction starts land at 0,2,5,7,10,12,15,17,20,...
    EXPECT_NE(joined.find("cyc=0 dev=cpu;"), std::string::npos);
    EXPECT_NE(joined.find("cyc=5 dev=cpu;"), std::string::npos);
    // Tie at cyc=10 (a 3-cycle STA started at 7 ends exactly on the timer
    // deadline): the timer was queued at boot, so FIFO dispatches it FIRST.
    const size_t t10 = joined.find("cyc=10 dev=timer;");
    const size_t c10 = joined.find("cyc=10 dev=cpu;");
    EXPECT_NE(t10, std::string::npos);
    EXPECT_NE(c10, std::string::npos);
    EXPECT_TRUE(t10 < c10);
    EXPECT_NE(joined.find("cyc=20 dev=timer;"), std::string::npos);
}

TEST(soc, halted_cpu_stops_scheduling_devices_continue) {
    std::array<uint8_t, 3> prog{0x01, 1, 0xFF};  // LDA #1; HALT
    soc::SoC s(prog);
    s.boot();
    s.run_until(500);
    EXPECT_TRUE(s.halted());
    // LDA@0 (ends 2), HALT@2 (ends 3): exactly two cpu events ever...
    uint64_t n = 0;
    for (const auto& line : s.event_log) {
        if (line.find("dev=cpu") != std::string::npos) ++n;
    }
    EXPECT_EQ(n, uint64_t{2});
    // ...while the timer keeps firing to the end of the window.
    EXPECT_EQ(s.timer.fire_count, uint64_t{50});  // 10..500
}
