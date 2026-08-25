#pragma once
// Guest-cycle event scheduler with DMA bus-preemption modeling.
//
// Design (curriculum §52/§56): everything runs headless in guest cycles;
// ties at identical timestamps resolve by INSERTION ORDER, which is what
// makes race scenarios deterministic (see 91_challenge).
#include <cstdint>
#include <functional>
#include <vector>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr u32 kCyclesPerLine = 1232;
constexpr u32 kHblankStart = 960;
constexpr u32 kVblankLine = 160;
constexpr u32 kTotalLines = 228;

// Full scheduler API is required by system glue even in skeleton builds.
//@LABS-BEGIN 1
//@LABS-SOLUTION
struct Scheduler {
    struct Event {
        u64 time;
        u64 seq;                  // insertion order breaks timestamp ties
        std::function<void()> fn;
    };

    std::vector<Event> events;
    u64 now = 0;
    u64 bus_busy_until = 0;

    void schedule(u64 time, std::function<void()> fn) {
        events.push_back({time, next_seq_++, std::move(fn)});
    }

    // DMA bursts make the bus unavailable: any event whose nominal time
    // falls inside the burst effectively fires when the burst ends.
    void begin_dma_burst(u64 start, u64 duration) {
        u64 end = start + duration;
        if (end > bus_busy_until) bus_busy_until = end;
    }

    u64 effective_time(const Event& e) const {
        return e.time < bus_busy_until ? bus_busy_until : e.time;
    }

    // Run every event whose effective time is <= limit, earliest first,
    // ties by insertion order. Updates `now` as it goes.
    void run_until(u64 limit) {
        for (;;) {
            if (events.empty()) return;
            size_t best = 0;
            for (size_t i = 1; i < events.size(); ++i) {
                const Event& a = events[i];
                const Event& b = events[best];
                u64 ta = effective_time(a), tb = effective_time(b);
                if (ta < tb || (ta == tb && a.seq < b.seq)) best = i;
            }
            u64 eff = effective_time(events[best]);
            if (eff > limit) return;
            now = eff;
            Event ev = events[best];
            events.erase(events.begin() + long(best));
            ev.fn();
        }
    }

 private:
    u64 next_seq_ = 0;
};
//@LABS-STUB
// TODO(1): implement the scheduler. Events carry (time, insertion seq,
// callback). `schedule` appends; `run_until` repeatedly picks the event
// with the smallest EFFECTIVE time (ties -> smaller seq), stops when the
// next effective time exceeds the limit, updates `now`, and invokes it.
struct Scheduler {
    u64 now = 0;
    void schedule(u64, std::function<void()>) {}   // TODO(1)
    void begin_dma_burst(u64, u64) {}              // TODO(1)
    void run_until(u64) {}                         // TODO(1)
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Cycles until a free-running 16-bit up-counter starting at `counter`
// overflows, at `prescale` cycles per tick. Period = 0x10000 - reload is
// the steady-state; this answers "when does the NEXT overflow fire" from
// an arbitrary live counter value.
inline u64 next_overflow_in(u16 counter, u64 prescale) {
    return (0x10000ull - counter) * prescale;
}
//@LABS-STUB
// TODO(2): cycles until overflow from live counter value `counter` with
// `prescale` cycles per tick: (0x10000 - counter) * prescale.
inline u64 next_overflow_in(u16 counter, u64 prescale) {
    (void)counter;
    (void)prescale;
    return 0;  // wrong on purpose
}
//@LABS-END

enum class VideoEvent { LineStart, HBlankStart, VBlankStart };

// Next display event strictly AFTER guest cycle `after` (frame = 228 lines
// x 1232 cycles; a line starts at n*1232, HBlank begins +960 into it, and
// entering line 160 reports VBlankStart).
void next_video_event(u64 after, u64* out_cycle, VideoEvent* out_kind);

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void next_video_event(u64 after, u64* out_cycle, VideoEvent* out_kind) {
    u64 line = after / kCyclesPerLine;
    u64 pos = after % kCyclesPerLine;
    if (pos < kHblankStart) {
        *out_cycle = line * kCyclesPerLine + kHblankStart;
        *out_kind = VideoEvent::HBlankStart;
        return;
    }
    u64 nl = line + 1;
    *out_cycle = nl * kCyclesPerLine;
    *out_kind =
        (nl == kVblankLine) ? VideoEvent::VBlankStart : VideoEvent::LineStart;
}
//@LABS-STUB
// TODO(3): implement per the comment above; strictly-after semantics.
inline void next_video_event(u64, u64* out_cycle, VideoEvent* out_kind) {
    *out_cycle = 0;                       // wrong on purpose
    *out_kind = VideoEvent::LineStart;
}
//@LABS-END

}  // namespace gba
