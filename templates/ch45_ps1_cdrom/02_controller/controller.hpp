#pragma once
//
// ch45 / 02_controller — asynchronous, event-driven CD-ROM controller
// core (psx-spx "CD-ROM Controller"). Commands handled here:
// GetStat (01h), Setloc (02h), Pause (09h), Init (0Ah).
//
// Timing model (documented, deterministic guest ticks):
//   * first response (INT3) is issued at t=0 relative to the command
//   * Init second phase (motor spin-up): +1200 ticks -> INT2
//   * Pause second phase:                 +250 ticks -> INT2
//
// Interrupt levels follow hardware semantics: INT3 = first response,
// INT2 = second response, INT1 = data ready / completion, INT5 = error.
// Only one interrupt is visible at a time; the guest ACKs to reveal the
// next queued one.

#include <cstdint>
#include <functional>
#include <vector>

#include "../01_disc_image/disc.hpp"

namespace cdrom {

constexpr uint8_t kCmdGetStat = 0x01;
constexpr uint8_t kCmdSetloc = 0x02;
constexpr uint8_t kCmdPause = 0x09;
constexpr uint8_t kCmdInit = 0x0A;
constexpr uint8_t kCmdSeekL = 0x15;

constexpr uint8_t kStatError = 0x01;
constexpr uint8_t kStatMotorOn = 0x02;
constexpr uint8_t kStatSeekError = 0x04;
constexpr uint8_t kStatShellOpen = 0x10;
constexpr uint8_t kStatRead = 0x20;
constexpr uint8_t kStatSeek = 0x40;
constexpr uint8_t kStatPlay = 0x80;

constexpr uint64_t kInitSpinupTicks = 1200;
constexpr uint64_t kPauseCompleteTicks = 250;

struct IrqEntry {
    uint8_t level;
    std::vector<uint8_t> response;
};

class CdRomController {
public:
    using LogSink =
        std::function<void(uint8_t level, const std::vector<uint8_t>& resp)>;

    explicit CdRomController(const DiscImage* disc = nullptr)
        : disc_(disc) {}

    // ---- guest-visible interface ------------------------------------
    void write_param(uint8_t v) { params_.push_back(v); }

    void cmd_init();    // exercise blocks 3/4
    void cmd_pause();

//@LABS-BEGIN 1
//@LABS-SOLUTION
    void issue(uint8_t cmd) {
        switch (cmd) {
            case kCmdGetStat:
                raise_irq(3, {stat_});
                break;
            case kCmdSetloc: {
                // Params arrive as BCD MM SS FF (three bytes expected).
                const unsigned mm = bcd(params_.size() > 0 ? params_[0] : 0);
                const unsigned ss = bcd(params_.size() > 1 ? params_[1] : 0);
                const unsigned ff = bcd(params_.size() > 2 ? params_[2] : 0);
                target_ = msf_to_lba(mm, ss, ff);
                params_.clear();
                raise_irq(3, {stat_});
                break;
            }
            case kCmdInit:
                cmd_init();
                break;
            case kCmdPause:
                cmd_pause();
                break;
            default:
                // Unimplemented command: error response (INT5), like the
                // real drive answering bad opcodes.
                params_.clear();
                raise_irq(5, {static_cast<uint8_t>(stat_ | kStatError)});
                break;
        }
    }
//@LABS-STUB
    // TODO(1): dispatch GetStat (respond with STAT, INT3) and Setloc
    // (decode three BCD params MM SS FF into target LBA via msf_to_lba,
    // respond INT3). Unknown opcodes answer INT5 with the ERROR stat bit.
    void issue(uint8_t cmd) {
        (void)cmd;
        params_.clear();
    }
//@LABS-END

    uint8_t irq_level() const {
        return irq_queue_.empty() ? 0 : irq_queue_.front().level;
    }
    bool resp_available() const { return !resp_fifo_.empty(); }
    uint8_t read_response() {
        const uint8_t v = resp_fifo_.front();
        resp_fifo_.erase(resp_fifo_.begin());
        return v;
    }
//@LABS-BEGIN 2
//@LABS-SOLUTION
    // ACK clears the CURRENT interrupt; the next queued one becomes
    // visible and its response bytes become readable.
    void ack_irq() {
        if (irq_queue_.empty()) return;
        irq_queue_.erase(irq_queue_.begin());
        refill_response_fifo();
    }
//@LABS-STUB
    // TODO(2): drop the current interrupt entry and expose the next
    // queued response (if any). ACK with an empty queue must be a no-op.
    void ack_irq() {}
//@LABS-END

    // ---- time --------------------------------------------------------
    void schedule(uint64_t delay, std::function<void()> fn) {
        events_.push_back({now_ + delay, ++seq_, std::move(fn)});
    }
    void tick(uint64_t n) {
        now_ += n;
        bool fired = true;
        while (fired) {
            fired = false;
            size_t best = SIZE_MAX;
            for (size_t i = 0; i < events_.size(); ++i)
                if (events_[i].at <= now_ &&
                    (best == SIZE_MAX ||
                     events_[i].at < events_[best].at ||
                     (events_[i].at == events_[best].at &&
                      events_[i].seq < events_[best].seq)))
                    best = i;
            if (best != SIZE_MAX) {
                auto fn = std::move(events_[best].fn);
                events_.erase(events_.begin() +
                              static_cast<long>(best));
                fn();
                fired = true;
            }
        }
    }
    uint64_t now() const { return now_; }

    // ---- shared state used by the read engine (chapter exercise 03) --
    uint8_t stat() const { return stat_; }
    void set_stat_bits(uint8_t bits) { stat_ |= bits; }
    void clear_stat_bits(uint8_t bits) { stat_ &= ~bits; }
    int32_t target_lba() const { return target_; }
    int32_t current_lba() const { return cur_loc_; }
    void set_current_lba(int32_t lba) { cur_loc_ = lba; }
    bool reading() const { return reading_; }
    void set_reading(bool r) { reading_ = r; }
    const DiscImage* disc() const { return disc_; }
    // Read-stream generation counter: bumping it invalidates scheduled
    // sector deliveries (Pause/Stop semantics).
    unsigned epoch() const { return epoch_; }
    void bump_epoch() { ++epoch_; }
    void set_log_sink(LogSink s) { log_sink_ = std::move(s); }

    void raise_irq(uint8_t level, std::vector<uint8_t> resp) {
        irq_queue_.push_back({level, resp});
        if (irq_queue_.size() == 1) refill_response_fifo();
        if (log_sink_) log_sink_(level, resp);
    }

private:
    static unsigned bcd(uint8_t v) {
        return (v >> 4) * 10u + (v & 0x0Fu);
    }
    void refill_response_fifo() {
        resp_fifo_ = irq_queue_.empty()
                         ? std::vector<uint8_t>{}
                         : irq_queue_.front().response;
    }

    struct Event {
        uint64_t at;
        unsigned seq;
        std::function<void()> fn;
    };

    const DiscImage* disc_;
    uint8_t stat_ = 0;
    int32_t target_ = 0;
    int32_t cur_loc_ = 0;
    bool reading_ = false;
    std::vector<uint8_t> params_;
    std::vector<IrqEntry> irq_queue_;
    std::vector<uint8_t> resp_fifo_;
    std::vector<Event> events_;
    LogSink log_sink_;
    uint64_t now_ = 0;
    unsigned seq_ = 0;
    unsigned epoch_ = 0;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Init: INT3 immediately (motor-on stat), second phase after spin-up.
inline void CdRomController::cmd_init() {
    set_stat_bits(kStatMotorOn);
    raise_irq(3, {stat_});
    schedule(kInitSpinupTicks, [this] { raise_irq(2, {stat_}); });
}
//@LABS-STUB
// TODO(3): respond INT3 with motor-on stat NOW, then schedule the second
// response (INT2) kInitSpinupTicks ticks in the future via schedule().
void CdRomController::cmd_init() {
    raise_irq(3, {static_cast<uint8_t>(kStatMotorOn)});
    // TODO(3): schedule the INT2 completion event.
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Pause: invalidates any streaming read (epoch bump), INT3 now, INT2
// after the documented pause-complete delay.
inline void CdRomController::cmd_pause() {
    bump_epoch();
    set_reading(false);
    clear_stat_bits(kStatRead | kStatSeek | kStatPlay);
    raise_irq(3, {stat_});
    schedule(kPauseCompleteTicks, [this] { raise_irq(2, {stat_}); });
}
//@LABS-STUB
// TODO(4): stop reading (bump epoch, clear READ/SEEK/PLAY stat bits),
// answer INT3 immediately and deliver the INT2 completion after
// kPauseCompleteTicks ticks.
void CdRomController::cmd_pause() {}
//@LABS-END

}  // namespace cdrom
