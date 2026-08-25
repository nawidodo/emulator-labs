#pragma once
#include <cstdint>

// Chapter 24 — APU channels (course model, deterministic, integer-only).
//
// All channels tick on explicit calls; the scheduler in 02 owns time. The
// DMC memory reader is a DOCUMENTED STUB: we track `bytes_remaining` and
// flag `needs_fetch()` when the sample buffer drains, but never steal CPU
// cycles for the fetch (level-trigger beyond this flag is out of scope).
namespace nes24apu {

// NTSC length-counter load table (entries are frames).
constexpr uint8_t kLengthTable[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

// NTSC noise period table (course constants; index = register value).
constexpr uint16_t kNoisePeriod[16] = {4, 8, 16, 32, 64, 96, 128, 160,
                                       202, 254, 380, 508, 762, 1016,
                                       2034, 4068};

// NTSC DMC rate table in CPU cycles (NESdev).
constexpr uint16_t kDmcRate[16] = {214, 190, 170, 159, 143, 127, 113, 107,
                                   95, 80, 71, 63, 53, 42, 36, 27};

//@LABS-BEGIN 1
//@LABS-SOLUTION
struct Pulse {
    int channel = 0;              // 0 or 1: sweep negate mode differs!
    bool enabled = false;
    int duty_mode = 0, duty_step = 0;
    uint16_t timer_period = 0;    // 11-bit; sequencer ticks every t+1... we
                                  // count down from timer_period directly
    uint16_t timer_counter = 0;
    // envelope
    bool halt = false, constant_volume = false, env_start = false;
    int env_divider = 0, env_decay = 0, env_volume = 0;
    // length
    int length_counter = 0;
    // sweep
    bool sweep_enabled = false, sweep_negate = false;
    int sweep_shift = 0, sweep_counter = 0, sweep_divider_period = 0;

    void write_reg(int reg, uint8_t v) {
        switch (reg & 3) {
            case 0:
                duty_mode = v >> 6;
                halt = (v & 0x20) != 0;
                constant_volume = (v & 0x10) != 0;
                env_volume = v & 0x0F;
                break;
            case 1:
                sweep_enabled = (v & 0x80) != 0;
                sweep_divider_period = (v >> 4) & 7;
                sweep_negate = (v & 0x08) != 0;
                sweep_shift = v & 7;
                break;
            case 2:
                timer_period = uint16_t((timer_period & 0x0700) | v);
                break;
            default:
                timer_period = uint16_t((timer_period & 0x00FF) |
                                        uint16_t((v & 7) << 8));
                env_start = true;
                if (enabled) length_counter = kLengthTable[v >> 3];
                break;
        }
    }

    // Quarter-frame clock.
    void tick_envelope() {
        if (env_start) {
            env_start = false;
            env_decay = 15;
            env_divider = env_volume;
        } else if (env_divider > 0) {
            --env_divider;
        } else {
            env_divider = env_volume;
            if (env_decay > 0) --env_decay;
            else if (halt) env_decay = 15;
        }
    }

    // Half-frame clock.
    void tick_length_and_sweep() {
        if (!halt && length_counter > 0) --length_counter;
        if (sweep_counter == 0) {
            sweep_counter = sweep_divider_period;
            if (sweep_enabled && sweep_shift > 0) {
                int delta = int(timer_period) >> sweep_shift;
                int target;
                bool mute = false;
                if (sweep_negate) {
                    // EXACT negate mode: pulse 1 subtracts the one's
                    // complement as-is; pulse 2 adds one back.
                    target = int(timer_period) - delta -
                             (channel == 0 ? 0 : -1);
                    if (target < 0) target = 0;
                } else {
                    target = int(timer_period) + delta;
                    mute = target > 0x7FF;
                }
                if (!mute && timer_period >= 8)
                    timer_period = uint16_t(target);
            }
        } else {
            --sweep_counter;
        }
    }

    void tick_timer() {
        if (timer_counter == 0) {
            timer_counter = timer_period;
            duty_step = (duty_step + 1) & 7;
        } else {
            --timer_counter;
        }
    }

    int output() const {
        static const uint8_t duty_tab[4][8] = {
            {0, 1, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 0, 0, 0, 0, 0},
            {0, 1, 1, 1, 1, 0, 0, 0},
            {0, 0, 1, 1, 0, 0, 0, 0}};
        if (!enabled || length_counter == 0 || timer_period < 8)
            return 0;
        return constant_volume ? env_volume : env_decay;
    }
};
//@LABS-STUB
// TODO(1): pulse channel — duty sequencer + 11-bit timer, envelope with
// start/divider/decay semantics, length counter via kLengthTable, and the
// sweep unit with EXACT negate mode (pulse 1 subtracts without correction,
// pulse 2 adds one back). Mute when length==0 or period<8.
struct Pulse {
    int channel = 0;
    bool enabled = false;

    void write_reg(int reg, uint8_t v) { (void)reg; (void)v; }  // TODO(1)
    void tick_envelope() {}                                     // TODO(1)
    void tick_length_and_sweep() {}                             // TODO(1)
    void tick_timer() {}                                        // TODO(1)
    int output() const { return 0; }                            // TODO(1)

    int env_decay = 0, length_counter = 0, env_divider = 0;
    int env_volume = 0, duty_mode = 0;
    int sweep_shift = 0;
    bool halt = false, constant_volume = false,
         sweep_enabled = false, sweep_negate = false, env_start = false;
    uint16_t timer_period = 0, timer_counter = 0;
    int duty_step = 0, sweep_counter = 0, sweep_divider_period = 0;
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
struct Triangle {
    bool enabled = false;
    uint16_t timer_period = 0, timer_counter = 0;
    int step = 0;                 // 0..31 through the 32-step sequence
    bool linear_reload = false;
    int linear_counter = 0, linear_reload_value = 0;
    bool control_flag = false;    // also = length halt
    int length_counter = 0;

    void write_reg(int reg, uint8_t v) {
        switch (reg & 3) {
            case 0:
                control_flag = (v & 0x80) != 0;
                linear_reload_value = v & 0x7F;
                break;
            case 2:
                timer_period = uint16_t((timer_period & 0x0700) | v);
                break;
            case 3:
                timer_period = uint16_t((timer_period & 0x00FF) |
                                        uint16_t((v & 7) << 8));
                linear_reload = true;
                if (enabled) length_counter = kLengthTable[v >> 3];
                break;
            default: break;
        }
    }

    void tick_linear() {
        if (linear_reload) {
            linear_counter = linear_reload_value;
        } else if (linear_counter > 0 && !control_flag) {
            --linear_counter;
        }
        if (!control_flag) linear_reload = false;
    }

    void tick_length() {
        if (!control_flag && length_counter > 0) --length_counter;
    }

    void tick_timer() {
        if (timer_counter == 0) {
            timer_counter = timer_period;
            // The sequencer halts while the length counter is zero.
            if (length_counter > 0 && linear_counter > 0)
                step = (step + 1) & 31;
        } else {
            --timer_counter;
        }
    }

    int output() const {
        // 15..0 then 0..15 across the 32 steps.
        return step < 16 ? 15 - step : step - 16;
    }
};
//@LABS-STUB
// TODO(2): triangle — 32-step sequencer gated by length AND linear
// counters, linear counter reload/control semantics, half-frame and
// quarter-frame clocks, 15..0/0..15 output ramp.
struct Triangle {
    bool enabled = false;
    void write_reg(int reg, uint8_t v) { (void)reg; (void)v; }  // TODO(2)
    void tick_linear() {}                                       // TODO(2)
    void tick_length() {}                                       // TODO(2)
    void tick_timer() {}                                        // TODO(2)
    int output() const { return 0; }                            // TODO(2)

    int step = 0, length_counter = 0, linear_counter = 0;
    int linear_reload_value = 0;
    bool control_flag = false, linear_reload = false, enabled_ = false;
    uint16_t timer_period = 0, timer_counter = 0;
};
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
struct Noise {
    bool enabled = false;
    uint16_t timer_period_index = 0, timer_counter = 0;
    uint16_t shift = 1;           // 15-bit LFSR; power-on state must be != 0
    bool short_mode = false, halt = false, constant_volume = false;
    int env_volume = 0, env_decay = 0, env_divider = 0, env_start_flag = 0;
    int length_counter = 0;

    void write_reg(int reg, uint8_t v) {
        switch (reg & 3) {
            case 0:
                halt = (v & 0x20) != 0;
                constant_volume = (v & 0x10) != 0;
                env_volume = v & 0x0F;
                break;
            case 2:
                timer_period_index = v & 0x0F;
                short_mode = (v & 0x80) != 0;
                break;
            case 3:
                env_start_flag = 1;
                if (enabled) length_counter = kLengthTable[v >> 3];
                break;
            default: break;
        }
    }

    void tick_envelope() {
        if (env_start_flag) {
            env_start_flag = 0;
            env_decay = 15;
            env_divider = env_volume;
        } else if (env_divider > 0) {
            --env_divider;
        } else {
            env_divider = env_volume;
            if (env_decay > 0) --env_decay;
            else if (halt) env_decay = 15;
        }
    }

    void tick_length() {
        if (!halt && length_counter > 0) --length_counter;
    }

    void tick_timer() {
        if (timer_counter == 0) {
            timer_counter = kNoisePeriod[timer_period_index & 0x0F];
            int bit = short_mode ? 6 : 1;
            uint16_t fb = uint16_t(((shift ^ (shift >> bit)) & 1));
            shift = uint16_t((shift >> 1) | (fb << 14));
        } else {
            --timer_counter;
        }
    }

    int output() const {
        if (!enabled || length_counter == 0 || (shift & 1)) return 0;
        return constant_volume ? env_volume : env_decay;
    }
};
//@LABS-STUB
// TODO(3): noise — 15-bit LFSR with short-mode tap at bit 6 (else bit 1),
// feedback into bit 14 right after the shift, period from kNoisePeriod,
// envelope + length like the pulse.
struct Noise {
    bool enabled = false;
    void write_reg(int reg, uint8_t v) { (void)reg; (void)v; }  // TODO(3)
    void tick_envelope() {}                                     // TODO(3)
    void tick_length() {}                                       // TODO(3)
    void tick_timer() {}                                        // TODO(3)
    int output() const { return 0; }                            // TODO(3)

    int length_counter = 0;
    uint16_t shift = 1, timer_counter = 0, timer_period_index = 0;
    bool short_mode = false, halt = false;
};
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
struct Dmc {
    bool enabled = false;
    int rate_index = 0, timer_counter = 0;
    int level = 0;                // 7-bit output, 0..126, steps of 2
    bool loop_flag = false;
    int bytes_remaining = 0;
    bool needs_fetch_ = false;

    void write_reg(int reg, uint8_t v) {
        switch (reg & 3) {
            case 0:
                rate_index = v & 0x0F;
                loop_flag = (v & 0x40) != 0;
                break;
            case 1: level = v & 0x7F; break;
            default: break;
        }
    }

    void restart(int sample_bytes) {
        bytes_remaining = sample_bytes;
        needs_fetch_ = sample_bytes > 0;
    }

    void tick_timer() {
        if (++timer_counter < kDmcRate[rate_index & 0x0F]) return;
        timer_counter = 0;
        // No sample bits buffered in this stub model: nothing to decode.
    }

    // DOCUMENTED STUB: fetches are flagged, never performed; no CPU cycles
    // are stolen. The machine treats this exactly like the reference does.
    bool needs_fetch() const { return needs_fetch_; }
    int output() const { return enabled ? level : 0; }
};
//@LABS-STUB
// TODO(4): DMC basics — 7-bit output level adjusted by decoded delta bits
// (+2/-2, clamped), rate timer, loop flag, and the documented STUB fetch
// contract: flag needs_fetch(), never steal cycles.
struct Dmc {
    bool enabled = false;
    void write_reg(int reg, uint8_t v) { (void)reg; (void)v; }  // TODO(4)
    void restart(int sample_bytes) { (void)sample_bytes; }      // TODO(4)
    void tick_timer() {}                                        // TODO(4)
    bool needs_fetch() const { return false; }                  // TODO(4)
    int output() const { return 0; }                            // TODO(4)

    int level = 0, bytes_remaining = 0, rate_index = 0, timer_counter = 0;
    bool loop_flag = false;
};
//@LABS-END

}  // namespace nes24apu
