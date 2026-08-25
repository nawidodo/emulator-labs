#pragma once
//
// ch45 / 03_read_engine — SeekL and streaming reads (ReadN / double-speed
// mode) on top of the chapter-45/02 controller.
//
// Timing model (documented, deterministic):
//   seek_ticks(delta_lba) = 100 + |delta_lba|        (linear lab model)
//   sector_ticks          = double_speed ? 50 : 100  (per delivered sector)
//   first INT1 lands after seek_ticks(|target - cur|) + sector_ticks,
//   subsequent sectors every sector_ticks.
//
// "ReadS" in this lab means reading with the double-speed bit set via
// Setmode-style state; the real drive exposes speed through Setmode
// rather than a second read command. XA ADPCM audio payloads are out of
// scope (form2 sectors are skipped, not delivered).

#include <cstdint>
#include <functional>

#include "../02_controller/controller.hpp"

namespace cdrom {

constexpr uint64_t kSeekBaseTicks = 100;
constexpr uint64_t kSectorTicksSlow = 100;
constexpr uint64_t kSectorTicksFast = 50;

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint64_t seek_ticks(int32_t delta_lba) {
    const int64_t d = delta_lba < 0 ? -int64_t(delta_lba) : delta_lba;
    return kSeekBaseTicks + static_cast<uint64_t>(d);
}

inline uint64_t sector_ticks(bool double_speed) {
    return double_speed ? kSectorTicksFast : kSectorTicksSlow;
}
//@LABS-STUB
// TODO(1): linear seek latency 100 + |delta| ticks and per-sector
// interval 50 ticks at double speed vs 100 otherwise.
uint64_t seek_ticks(int32_t delta_lba) {
    (void)delta_lba;
    return 0;  // wrong on purpose
}
uint64_t sector_ticks(bool double_speed) {
    (void)double_speed;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// SeekL (15h): SEEK stat during the move, INT3 now, completion INT2 when
// the head arrives; the current position snaps to the target.
inline void cmd_seekl(CdRomController& c) {
    const int32_t delta = c.target_lba() - c.current_lba();
    c.set_stat_bits(kStatSeek);
    c.raise_irq(3, {c.stat()});
    c.schedule(seek_ticks(delta), [&c] {
        c.clear_stat_bits(kStatSeek);
        c.set_current_lba(c.target_lba());
        c.raise_irq(2, {c.stat()});
    });
}
//@LABS-STUB
// TODO(2): set the SEEK stat bit, answer INT3 immediately, then schedule
// the arrival event after seek_ticks(target - current): clear SEEK, snap
// current position to target and raise INT2.
void cmd_seekl(CdRomController& c) {
    (void)c;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// One sector delivery: INT1 with an empty response payload (real data
// moves via DMA), current location advanced, next sector scheduled while
// the drive keeps reading.
inline void deliver_sector(CdRomController& c, bool double_speed,
                           unsigned epoch) {
    if (epoch != c.epoch()) return;   // stream was aborted (Pause)
    c.raise_irq(1, {});
    c.set_current_lba(c.current_lba() + 1);
    if (c.reading()) {
        const unsigned next_epoch = epoch;
        c.schedule(sector_ticks(double_speed), [&c, double_speed,
                                                next_epoch] {
            deliver_sector(c, double_speed, next_epoch);
        });
    }
}

// ReadN (06h): stream sectors starting at the current location. First
// INT1 after seek + one sector interval, then one INT1 per sector.
inline void start_read(CdRomController& c, bool double_speed) {
    if (!c.disc()) return;
    const int32_t hop =
        c.target_lba() > c.current_lba()
            ? c.target_lba() - c.current_lba()
            : 0;
    c.set_stat_bits(kStatRead);
    c.set_reading(true);
    c.raise_irq(3, {c.stat()});
    // Move to target (if ahead), then deliver the first sector there.
    const unsigned epoch = c.epoch();
    c.schedule(
        seek_ticks(hop) + sector_ticks(double_speed),
        [&c, double_speed, epoch] {
            if (c.epoch() != epoch) return;
            c.set_current_lba(c.target_lba());  // head arrived
            deliver_sector(c, double_speed, epoch);
        });
}
//@LABS-STUB
// TODO(3): implement BOTH functions per the comments above:
//  - deliver_sector: raise INT1 (empty payload), advance cur_loc by one,
//    schedule the next delivery after sector_ticks while still reading;
//  - start_read: require a disc, set READ stat + reading state, answer
//    INT3 immediately, schedule the first delivery after seek+sector
//    latency (hop = max(0, target - cur)).
void deliver_sector(CdRomController& c, bool double_speed, unsigned epoch) {
    (void)c; (void)double_speed; (void)epoch;
}
void start_read(CdRomController& c, bool double_speed) {
    (void)c; (void)double_speed;
}
//@LABS-END

}  // namespace cdrom
