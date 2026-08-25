#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace chip8 {

// Fixed teaching assumption, documented here because every golden trace and
// frame hash in the course depends on it:
//
//   the CHIP-8 CPU executes exactly kCyclesPerSecond = 600 instructions per
//   second, delay/sound timers tick down at 60 Hz, and the display refreshes
//   at 60 fps. That makes every timing relationship an exact integer:
//   10 instructions per timer tick, 10 instructions per frame.
//
// Real CHIP-8 hosts ran anywhere from ~500 to ~1000 instructions per second;
// picking one fixed rate is what makes headless replay deterministic.
inline constexpr uint32_t kCyclesPerSecond = 600;
inline constexpr uint32_t kFramesPerSecond = 60;
inline constexpr uint32_t kCyclesPerTimerTick = kCyclesPerSecond / kFramesPerSecond;  // 10
inline constexpr uint32_t kCyclesPerFrame = kCyclesPerSecond / kFramesPerSecond;      // 10

// Fired on beep state transitions. `started == true` when sound left zero,
// false when it reached zero again. The host decides what "beep" means; in
// this course we only RECORD transitions — nothing audible, ever.
using BeepHook = std::function<void(bool started)>;

struct Timers {
    uint8_t delay = 0;
    uint8_t sound = 0;

    // Beep transition hook. May be empty (no observer).
    BeepHook on_beep;

    void set_delay(uint8_t v) { delay = v; }

    void set_sound(uint8_t v) {
        const bool was_silent = sound == 0;
        sound = v;
        if (was_silent && sound != 0 && on_beep) on_beep(true);
    }

    bool beeping() const { return sound != 0; }

    // Advances timer time by `cycles` CPU cycles. Internally accumulates
    // fractional ticks so any cycle count works, not just multiples of ten:
    // at our fixed 600 cps a tick fires every 10 cycles.
    void tick_cycles(uint64_t cycles) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        accumulator_ += cycles * uint64_t{kFramesPerSecond};
        while (accumulator_ >= kCyclesPerSecond) {
            accumulator_ -= kCyclesPerSecond;
            tick_once();
        }
//@LABS-STUB
        // TODO(1): accumulate fractional 60 Hz ticks and call tick_once()
        // once per whole tick. Hint: scale by kFramesPerSecond to stay in
        // integers for arbitrary cycle counts.
        (void)cycles;
//@LABS-END
    }

private:
    uint64_t accumulator_ = 0;

    // One 60 Hz timer tick: both timers decrement toward zero. Timers stop
    // at zero — they are countdowns, not wrapping counters.
    void tick_once() {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if (delay > 0) --delay;
        if (sound > 0) {
            --sound;
            if (sound == 0 && on_beep) on_beep(false);  // beep ended
        }
//@LABS-STUB
        // TODO(2): decrement delay and sound by exactly one, clamping at
        // zero, and fire on_beep(false) when sound REACHES zero.
        ++delay;       // wrong on purpose
        if (sound > 0) --delay;
//@LABS-END
    }

public:
    // Test seam: how many whole ticks are currently banked.
    uint64_t pending_accumulator() const { return accumulator_; }
};

// Recording observer for the beep hook. Tests and the headless runner use
// this instead of an actual speaker.
struct BeepRecorder {
    std::vector<bool> events;  // true = beep started, false = beep ended

    void attach(Timers& t) {
        t.on_beep = [this](bool started) { events.push_back(started); };
    }

    bool started_count_equals(int n) const {
        int c = 0;
        for (bool e : events)
            if (e) ++c;
        return c == n;
    }
};

}  // namespace chip8
