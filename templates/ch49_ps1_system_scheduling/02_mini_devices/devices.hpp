// Stand-in devices for the ch49 system-scheduling chapter.
//
// The REAL PS1 subsystems are authored by sibling chapters; this chapter
// integrates components behind a common event scheduler, so each device
// here is a minimal but honest model of the timing behavior that matters:
//
//   GPU  — GP0 writes queue command execution; the queue drains via
//          scheduler events at the pixel-clock ratio (53.2224 MHz video
//          dot clock = CPU x 11/7); STAT bit 28 reports busy; an optional
//          interrupt fires when the queue goes idle (INTC line 1).
//   DMA  — a channel drains N words at kDmaCyclesPerWord cycles each and
//          STALLS the CPU until done (INTC line 3 on completion).
//   CD   — a sector read completes after kCdSectorCycles (exactly 26 SPU
//          samples) and raises INTC line 2.
//   SPU  — a sample-period event every 768 CPU cycles (44100 Hz exactly);
//          while enabled it latches INTC line 9 on each sample boundary.
//   INTC — status/mask latch; assertion ORDER is deterministic because
//          same-instant events dispatch FIFO from the scheduler.
//
// Every device advances ONLY via scheduler events — no polling loops.
// Devices are bound to the shared Log/Intc once (see bind()); scheduling
// calls are templates over the scheduler type so the debugging exercise
// can swap in its instrumented scheduler.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "../01_scheduler_core/scheduler.hpp"

namespace ps1sys {

using Log = std::vector<std::string>;

// Master clock: 33.8688 MHz CPU. Derived ratios used throughout:
constexpr uint64_t kMasterHz = 33'868'800ull;
constexpr uint64_t kSpuSampleCycles = 768;        // 33.8688M / 44100 exact
constexpr uint64_t kVideoFrameCycles = 564480ull; // 33.8688M / 60
constexpr uint32_t kDmaCyclesPerWord = 6;
constexpr uint32_t kCdSectorCycles = 19968;       // 26 samples: lets CD and
                                                  // SPU deadlines align

// MMIO map (byte addresses, word-accessed by SW/LW). Deliberately kept
// inside ONE 64 KiB page so boot programs reach every register with a
// single base register plus a 16-bit offset (the real PS1 scatters these
// across 0x1F801xxx and needs a second LUI/ORI pair).
constexpr uint32_t kMmioBase = 0x1F800000u;
constexpr uint32_t kIStat = kMmioBase + 0x070u;   // INTC status (write=ack)
constexpr uint32_t kIMask = kMmioBase + 0x074u;   // INTC mask
constexpr uint32_t kDmaBcr = kMmioBase + 0x0C4u;  // stand-in: word count
constexpr uint32_t kDmaChcr = kMmioBase + 0x0C8u; // stand-in: write 1=start
constexpr uint32_t kCdCmd = kMmioBase + 0x800u;   // write nonzero = read
constexpr uint32_t kGpuGp0 = kMmioBase + 0x810u;  // store GP0/load GPSTAT
constexpr uint32_t kGpuGp1 = kMmioBase + 0x814u;  // control
constexpr uint32_t kSpuCtrl = kMmioBase + 0xC00u; // bit0: sample IRQ enable
constexpr uint32_t kMilestone = kMmioBase + 0xFF0u; // write logs milestone
constexpr int kLineGpu = 1;
constexpr int kLineCd = 2;
constexpr int kLineDma = 3;
constexpr int kLineSpu = 9;

inline void log_line(Log& log, const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log.emplace_back(buf);
}

inline void log_event(Log& log, uint64_t cyc, const char* name,
                      const char* detail_fmt = nullptr, ...) {
    if (!detail_fmt) {
        log_line(log, "cyc=%llu evt=%s",
                 static_cast<unsigned long long>(cyc), name);
        return;
    }
    char detail[96];
    va_list args;
    va_start(args, detail_fmt);
    std::vsnprintf(detail, sizeof(detail), detail_fmt, args);
    va_end(args);
    log_line(log, "cyc=%llu evt=%s %s",
             static_cast<unsigned long long>(cyc), name, detail);
}

// ---------------------------------------------------------------------------
// INTC — interrupt controller state. Latch order is guaranteed by the
// scheduler's FIFO tie-break: two IRQs due in the same dispatch batch call
// assert() in event-insertion order, and the log records that order.
class Intc {
public:
    void bind(Log* log) { log_ = log; }
    void reset() { status_ = 0; mask_ = 0; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Latch request line `line`. Every raise appends one event line so
    // the log pins WHEN each device raised its request and in what order
    // relative to its dispatch batch (status itself stays OR-ed until
    // software acks).
    void assert(uint64_t cyc, int line, const char* src) {
        status_ |= 1u << line;
        log_event(*log_, cyc, "latch", "line=%d src=%s", line, src);
    }
//@LABS-STUB
    // TODO(1): latch the request bit for `line` into the status register
    // and append one "latch" event line (only when the bit transitions
    // 0->1). Leaving status unchanged still compiles so tests run RED.
    void assert(uint64_t cyc, int line, const char* src) {
        (void)cyc; (void)line; (void)src;
    }
//@LABS-END

    void ack(uint32_t bits) { status_ &= ~bits; }
    void set_mask(uint32_t m) { mask_ = m; }
    uint32_t status() const { return status_; }
    uint32_t mask() const { return mask_; }
    uint32_t asserted() const { return status_ & mask_; }

private:
    Log* log_ = nullptr;
    uint32_t status_ = 0;
    uint32_t mask_ = 0;
};

// ---------------------------------------------------------------------------
// GPU — busy model over the pixel clock.
class Gpu {
public:
    void bind(Log* log, Intc* intc) { log_ = log; intc_ = intc; }
    void reset() {
        queue_.clear();
        busy_ = false;
        irq_on_done_ = false;
        rem_ = 0;
    }

    // GPU dot clock 53.2224 MHz runs at 11/7 times the CPU clock, so
    // `pixels` pixels cost pixels*7/11 CPU cycles. The remainder carries
    // forward so long command streams never drift from the master clock.
//@LABS-BEGIN 2
//@LABS-SOLUTION
    uint64_t cpu_cycles_for(uint32_t pixels) {
        const uint64_t num = rem_ + static_cast<uint64_t>(pixels) * 7ull;
        const uint64_t cyc = num / 11ull;
        rem_ = num % 11ull;
        return cyc;
    }
//@LABS-STUB
    // TODO(2): convert pixels to CPU cycles at 11/7 pixels-per-CPU-cycle,
    // carrying the fractional remainder in rem_ Bresenham-style so no
    // fractional time is lost across commands. Returning 0 compiles but
    // collapses all GPU timing onto "now".
    uint64_t cpu_cycles_for(uint32_t pixels) {
        (void)pixels;
        return 0;
    }
//@LABS-END

    // GP0 write: queue a command word. Bits [31:20] tag the command,
    // [19:0] carry the pixel count used for its execution duration.
    template <class S>
    void gp0(S& sched, uint64_t now, uint32_t w) {
        queue_.push_back(w);
        if (!busy_) {
            busy_ = true;
            log_event(*log_, now, "gpu_cmd", "pixels=%u cmd=0x%08X",
                      w & 0xFFFFFu, w);
            schedule_drain(sched, now);
        }
    }

    void gp1(uint32_t w) {
        if ((w >> 24) == 0x04) irq_on_done_ = (w & 1u) != 0;
        else if (w == 0) reset();  // soft reset clears queued commands
    }

    // Drain event: retire the head command, keep draining, or go idle
    // (optionally raising the GPU interrupt on the idle transition).
    template <class S>
    void on_drain(S& sched, uint64_t now) {
        if (!queue_.empty()) queue_.erase(queue_.begin());
        if (!queue_.empty()) {
            schedule_drain(sched, now);
            return;
        }
        busy_ = false;
        log_event(*log_, now, "gpu_idle");
        if (irq_on_done_) intc_->assert(now, kLineGpu, "gpu");
    }

    bool busy() const { return busy_; }
    size_t queued() const { return queue_.size(); }
    uint32_t head_pixels() const {
        return queue_.empty() ? 0u : queue_.front() & 0xFFFFFu;
    }
    uint32_t stat() const { return busy_ ? 0x10000000u : 0u; }  // bit 28

private:
    template <class S>
    void schedule_drain(S& sched, uint64_t now) {
        // Compute the duration ONCE here: the accumulator consumes its
        // remainder per call, so a second call would corrupt the clock.
        const uint64_t deadline = now + cpu_cycles_for(head_pixels());
        sched.schedule(deadline,
                       [this, &sched, deadline] {
                           on_drain(sched, deadline);
                       },
                       "gpu_drain");
    }

    Log* log_ = nullptr;
    Intc* intc_ = nullptr;
    std::vector<uint32_t> queue_;
    bool busy_ = false;
    bool irq_on_done_ = false;
    uint64_t rem_ = 0;
};

// ---------------------------------------------------------------------------
// DMA — stall model: the CPU pauses while the channel drains.
class Dma {
public:
    void bind(Log* log, Intc* intc) { log_ = log; intc_ = intc; }
    void reset() { busy_ = false; words_ = 0; done_time_ = 0; }

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Start draining `words` words at kDmaCyclesPerWord cycles each. While
    // busy_, the CPU reschedules itself past done_time() instead of
    // executing — the bus stall made explicit as two events that cannot
    // overlap. Completion raises INTC line 3.
    template <class S>
    bool start(S& sched, uint64_t now, uint32_t words) {
        if (busy_ || words == 0) return false;
        busy_ = true;
        words_ = words;
        done_time_ = now + static_cast<uint64_t>(words) * kDmaCyclesPerWord;
        sched.schedule(done_time_, [this] { finish(); }, "dma_done");
        log_event(*log_, now, "dma_start", "words=%u", words);
        return true;
    }
//@LABS-STUB
    // TODO(3): reject a second start while busy or when words==0;
    // otherwise record words_, compute done_time_ =
    // now + words*kDmaCyclesPerWord, mark busy_, schedule the dma_done
    // event (it must call finish()), and log "dma_start". Returning false
    // always compiles so tests run RED.
    template <class S>
    bool start(S& sched, uint64_t now, uint32_t words) {
        (void)sched; (void)now; (void)words;
        return false;
    }
//@LABS-END
    void finish() {
        busy_ = false;
        log_event(*log_, done_time_, "dma_done", "words=%u", words_);
        intc_->assert(done_time_, kLineDma, "dma");
    }

    bool busy() const { return busy_; }
    uint64_t done_time() const { return done_time_; }
    uint32_t words() const { return words_; }

private:
    Log* log_ = nullptr;
    Intc* intc_ = nullptr;
    bool busy_ = false;
    uint32_t words_ = 0;
    uint64_t done_time_ = 0;
};

// ---------------------------------------------------------------------------
// CD — latency model: a sector read completes after a fixed delay and
// raises IRQ2 (this stand-in delivers status, not data streaming).
class Cd {
public:
    void bind(Log* log, Intc* intc) { log_ = log; intc_ = intc; }
    void reset() { pending_ = false; lba_ = 0; }

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // One outstanding read at a time, like the drive's single command
    // slot. Completion lands exactly kCdSectorCycles later and latches
    // IRQ2 — an event other devices can be aligned against.
    template <class S>
    bool read_sector(S& sched, uint64_t now) {
        if (pending_) return false;
        pending_ = true;
        const uint64_t deadline = now + kCdSectorCycles;
        sched.schedule(deadline, [this, deadline] { finish(deadline); },
                       "cd_done");
        return true;
    }
//@LABS-STUB
    // TODO(4): refuse a second concurrent read; otherwise mark pending_,
    // schedule cd_done exactly kCdSectorCycles from `now` (its handler
    // must call finish()), and report the kick was accepted. Returning
    // false always compiles so tests run RED.
    template <class S>
    bool read_sector(S& sched, uint64_t now) {
        (void)sched; (void)now;
        return false;
    }
//@LABS-END

    void finish(uint64_t now) {
        pending_ = false;
        ++lba_;
        log_event(*log_, now, "cd_done", "lba=%u", lba_);
        intc_->assert(now, kLineCd, "cd");
    }

    bool pending() const { return pending_; }
    uint32_t lba() const { return lba_; }

private:
    Log* log_ = nullptr;
    Intc* intc_ = nullptr;
    bool pending_ = false;
    uint32_t lba_ = 0;
};

// ---------------------------------------------------------------------------
// SPU — sample-period events: one tick every kSpuSampleCycles (768), i.e.
// exactly 44100 Hz on the 33.8688 MHz master clock.
class Spu {
public:
    void bind(Log* log, Intc* intc) { log_ = log; intc_ = intc; }
    void reset() { ctrl_ = 0; sample_index_ = 0; }

    void set_ctrl(uint32_t v) { ctrl_ = v; }
    bool irq_enabled() const { return (ctrl_ & 1u) != 0; }
    uint64_t sample_index() const { return sample_index_; }

//@LABS-BEGIN 5
//@LABS-SOLUTION
    // One sample boundary elapsed. The SYSTEM owns the recurrence (it
    // re-schedules the next tick at deadline+768); this handler advances
    // the sample counter and, while enabled, latches IRQ9 per sample.
    void on_sample_tick(uint64_t now) {
        ++sample_index_;
        if (irq_enabled()) intc_->assert(now, kLineSpu, "spu");
    }
//@LABS-STUB
    // TODO(5): advance sample_index_ by one and, when ctrl_ enables the
    // sample-period interrupt, latch INTC line 9 with source "spu".
    void on_sample_tick(uint64_t now) {
        (void)now;
    }
//@LABS-END

private:
    Log* log_ = nullptr;
    Intc* intc_ = nullptr;
    uint32_t ctrl_ = 0;
    uint64_t sample_index_ = 0;
};

}  // namespace ps1sys
