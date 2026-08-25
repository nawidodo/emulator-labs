#define LABSTEST_MAIN
#include "labstest.hpp"
#include "read_engine.hpp"

#include <fstream>
#include <vector>

using namespace cdrom;

namespace {
// Build a 64-sector synthetic MODE2 image on disk under /tmp.
struct TempDisc {
    DiscImage disc;
    std::string cue_path, bin_path;
    explicit TempDisc(unsigned n = 64) {
        cue_path = "/tmp/labs_ch45_disc.cue";
        bin_path = "/tmp/labs_ch45_disc.bin";
        std::ofstream bf(bin_path, std::ios::binary);
        for (unsigned i = 0; i < n; ++i) {
            std::vector<uint8_t> s(kRawSectorSize, 0);
            s[0] = 0x00;
            for (unsigned j = 1; j < 11; ++j) s[j] = 0xFF;
            s[11] = 0x00;
            unsigned m, sec, f;
            lba_to_msf(static_cast<int32_t>(i), m, sec, f);
            s[12] = static_cast<uint8_t>(((m / 10) << 4) | (m % 10));
            s[13] = static_cast<uint8_t>(((sec / 10) << 4) | (sec % 10));
            s[14] = static_cast<uint8_t>(((f / 10) << 4) | (f % 10));
            s[15] = 0x02;
            for (unsigned j = 0; j < kUserDataSize; ++j)
                s[24 + j] = static_cast<uint8_t>((i * 7 + j) & 0xFF);
            bf.write(reinterpret_cast<const char*>(s.data()),
                     static_cast<std::streamsize>(s.size()));
        }
        bf.close();
        std::ofstream cf(cue_path);
        cf << "FILE \"" << bin_path << "\" BINARY\n"
           << "  TRACK 01 MODE2/2352\n"
           << "    INDEX 01 00:02:00\n";
        cf.close();
        disc.load(cue_path, bin_path);
    }
    ~TempDisc() {
        remove(cue_path.c_str());
        remove(bin_path.c_str());
    }
};

uint8_t bcd(unsigned v) {
    return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
}
}  // namespace

TEST(read, latency_tables) {
    EXPECT_EQ(seek_ticks(0), 100u);       // pure base cost
    EXPECT_EQ(seek_ticks(50), 150u);
    EXPECT_EQ(seek_ticks(-30), 130u);     // direction irrelevant
    EXPECT_EQ(sector_ticks(false), 100u);
    EXPECT_EQ(sector_ticks(true), 50u);   // double speed ("ReadS")
}

TEST(read, seekl_arrives_after_linear_latency) {
    CdRomController c;
    c.set_current_lba(0);
    c.write_param(bcd(0));
    c.write_param(bcd(3));
    c.write_param(bcd(25));   // MSF 0:03:25 -> LBA 100
    c.issue(kCmdSetloc);
    c.ack_irq();

    cmd_seekl(c);
    EXPECT_EQ(c.irq_level(), 3);
    EXPECT_NE(c.stat() & kStatSeek, 0);
    c.ack_irq();

    c.tick(seek_ticks(100) - 1);
    EXPECT_EQ(c.irq_level(), 0);          // not there yet
    c.tick(1);
    EXPECT_EQ(c.irq_level(), 2);
    EXPECT_EQ(c.current_lba(), 100);      // snapped to target
    EXPECT_EQ(c.stat() & kStatSeek, 0);
}

TEST(read, readn_streams_at_single_speed_intervals) {
    TempDisc td(64);
    CdRomController c(&td.disc);
    c.set_current_lba(0);
    c.write_param(0x00); c.write_param(0x02); c.write_param(0x10);  // MSF 0:2:10 -> LBA 10
    c.issue(kCmdSetloc);
    c.ack_irq();

    start_read(c, /*double_speed=*/false);
    EXPECT_EQ(c.irq_level(), 3);          // command ACK first
    c.ack_irq();
    EXPECT_NE(c.stat() & kStatRead, 0);

    // First sector: seek(10)=110 + interval 100 => t=210, AT the target.
    c.tick(209);
    EXPECT_EQ(c.irq_level(), 0);
    c.tick(1);
    EXPECT_EQ(c.irq_level(), 1);          // INT1: data ready
    c.ack_irq();
    EXPECT_EQ(c.current_lba(), 11);       // snapped to target then advanced

    // Second sector after another 100 ticks.
    c.tick(100);
    EXPECT_EQ(c.irq_level(), 1);
    c.ack_irq();
    EXPECT_EQ(c.current_lba(), 12);
}

TEST(read, double_speed_halves_sector_interval) {
    TempDisc td(64);
    CdRomController c(&td.disc);
    c.set_current_lba(0);
    c.write_param(0x00); c.write_param(0x02); c.write_param(0x10);
    c.issue(kCmdSetloc);
    c.ack_irq();

    start_read(c, /*double_speed=*/true);   // "ReadS" mode
    c.ack_irq();                            // INT3
    c.tick(seek_ticks(10) + sector_ticks(true));  // 110 + 50 = 160
    EXPECT_EQ(c.irq_level(), 1);
    c.ack_irq();
    EXPECT_EQ(c.current_lba(), 11);

    c.tick(50);                             // fast interval
    EXPECT_EQ(c.irq_level(), 1);
}

TEST(read, pause_during_read_stops_the_stream) {
    TempDisc td(64);
    CdRomController c(&td.disc);
    c.set_current_lba(0);
    c.write_param(0x00); c.write_param(0x02); c.write_param(0x10);
    c.issue(kCmdSetloc);
    c.ack_irq();
    start_read(c, false);
    c.ack_irq();                            // INT3
    c.tick(210);
    EXPECT_EQ(c.irq_level(), 1);            // first sector ready
    c.ack_irq();

    c.issue(kCmdPause);                     // stops streaming immediately
    c.ack_irq();                            // pause INT3
    const int32_t frozen = c.current_lba();
    c.tick(kPauseCompleteTicks + 500);
    EXPECT_EQ(c.irq_level(), 2);            // only the pause completion
    c.ack_irq();
    EXPECT_EQ(c.irq_level(), 0);
    EXPECT_EQ(c.current_lba(), frozen);     // no further deliveries
}
