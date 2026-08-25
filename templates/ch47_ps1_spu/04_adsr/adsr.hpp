#pragma once
#include <algorithm>
#include <cstdint>

namespace spu {

// ---------------------------------------------------------------------------
// SPU ADSR envelope.
//
// Register layout (voice ADSR1/ADSR2, published PSX-SPX):
//   ADSR1: bits  0-3  sustain level (sl)   target = (sl+1) * 0x800
//          bits  4-7  decay rate (dr)       - decay is always exponential
//          bits  8-14 attack rate (ar)
//          bit  15    attack mode (0 lin, 1 exp)
//   ADSR2: bits  0-4  release rate (rr)
//          bit   5    release mode (0 lin, 1 exp)
//          bits  6-12 sustain rate (sr)
//          bit  13    sustain direction (0 dec, 1 inc)
//          bit  14    sustain mode (0 lin, 1 exp)
//
// Rate -> timing model (documented approximation of the published tables):
//   every 7-bit rate r splits into group g = r>>2 and sub s = r&3,
//     period = 1 << max(0, 11 - g)     (samples between updates)
//     step   = 7 - s                   (update magnitude)
//   Doubling-time halves every +4 rates, matching the published table's
//   shape. Rates < 12 freeze the phase (documented simplification of the
//   "effectively infinite" bottom range on real hardware). The narrower
//   register fields (dr is 4-bit, rr is 5-bit) index the same table
//   directly, so they cover the slow end - exactly like real decay.
//
// Update rules (documented exponential approximation):
//   linear increase:  l += step            (clamped at 32767 or target)
//   linear decrease:  l -= step            (clamped at 0)
//   exp increase:     l += max(1, (l * step) >> 6)
//   exp decrease:     l -= max(1, (l * step) >> 6)
//
// The level lives in 0..32767 and is applied as gain >> 15 by the mixer
// (ex05).
// ---------------------------------------------------------------------------

constexpr int kAdsrMax = 32767;

enum class AdsrPhase { Off, Attack, Decay, Sustain, Release };

struct RateStep {
    uint16_t period;  // samples between updates (0 = frozen)
    uint8_t step;     // magnitude of one update
};

struct AdsrParams {
    unsigned ar = 0, dr = 0, sr = 0, rr = 0;
    bool attack_exp = false;
    bool release_exp = false;
    bool sustain_inc = false;
    bool sustain_exp = false;
    int sustain_level = 0x800;

    static AdsrParams unpack(uint16_t adsr1, uint16_t adsr2);
};

class Adsr {
public:
    void key_on(const AdsrParams& p);   // Off -> Attack
    void key_off();                     // any phase -> Release

    // Advances one SPU sample clock (44100 Hz). Returns the level 0..32767.
    int tick();

    int level() const { return level_; }
    AdsrPhase phase() const { return phase_; }

private:
    void apply_update(bool inc, bool exp, int limit);

    AdsrParams p_{};
    AdsrPhase phase_ = AdsrPhase::Off;
    int level_ = 0;
    uint32_t counter_ = 0;
};

//@LABS-BEGIN 8
//@LABS-SOLUTION
inline RateStep decode_rate(unsigned r) {
    if (r < 12) return {0, 0};  // frozen
    const unsigned g = r >> 2;
    const unsigned shift = g >= 11 ? 0u : 11u - g;
    return {static_cast<uint16_t>(1u << shift),
            static_cast<uint8_t>(7 - (r & 3))};
}

inline AdsrParams AdsrParams::unpack(uint16_t adsr1, uint16_t adsr2) {
    AdsrParams p;
    p.ar = (adsr1 >> 8) & 0x7F;
    p.dr = (adsr1 >> 4) & 0xF;
    const unsigned sl = adsr1 & 0xF;
    p.sustain_level = static_cast<int>(sl + 1) * 0x800;
    p.attack_exp = adsr1 & 0x8000;
    p.rr = adsr2 & 0x1F;
    p.release_exp = adsr2 & 0x20;
    p.sr = (adsr2 >> 6) & 0x7F;
    p.sustain_inc = adsr2 & 0x2000;
    p.sustain_exp = adsr2 & 0x4000;
    return p;
}
//@LABS-STUB
// TODO(8): decode_rate splits r into group g = r>>2, sub s = r&3 and
// returns { period = 1 << max(0, 11-g), step = 7-s }, with rates < 12
// returning {0, 0} (frozen). unpack() slices the two register words per
// the layout comment above (sustain level target = (sl+1)*0x800).
inline RateStep decode_rate(unsigned) {
    // TODO(8): implement the published-style rate table.
    return {0, 0};
}

inline AdsrParams AdsrParams::unpack(uint16_t, uint16_t) {
    // TODO(8): unpack ADSR1/ADSR2 bitfields.
    return {};
}
//@LABS-END

//@LABS-BEGIN 9
//@LABS-SOLUTION
inline void Adsr::key_on(const AdsrParams& p) {
    p_ = p;
    level_ = 0;
    counter_ = 0;
    phase_ = AdsrPhase::Attack;
}

inline void Adsr::key_off() {
    if (phase_ != AdsrPhase::Off) phase_ = AdsrPhase::Release;
}

inline void Adsr::apply_update(bool inc, bool exp, int limit) {
    const RateStep rs = [&] {
        switch (phase_) {
            case AdsrPhase::Attack: return decode_rate(p_.ar);
            case AdsrPhase::Decay: return decode_rate(p_.dr);
            case AdsrPhase::Sustain: return decode_rate(p_.sr);
            default: return decode_rate(p_.rr);
        }
    }();
    if (rs.period == 0) return;  // frozen rate: level never advances
    if (++counter_ % rs.period != 0) return;
    if (inc) {
        if (exp) {
            const int delta = std::max(1, (level_ * rs.step) >> 6);
            level_ = level_ + delta > limit ? limit : level_ + delta;
        } else {
            level_ = level_ + rs.step > limit ? limit : level_ + rs.step;
        }
    } else {
        if (exp) {
            const int delta = std::max(1, (level_ * rs.step) >> 6);
            level_ = delta > level_ ? 0 : level_ - delta;
        } else {
            level_ = rs.step > level_ ? 0 : level_ - rs.step;
        }
    }
}

inline int Adsr::tick() {
    switch (phase_) {
        case AdsrPhase::Off:
        case AdsrPhase::Release:
            break;
        case AdsrPhase::Attack:
            apply_update(true, p_.attack_exp, kAdsrMax);
            if (level_ >= kAdsrMax) {
                level_ = kAdsrMax;
                phase_ = AdsrPhase::Decay;
                counter_ = 0;
            }
            break;
        case AdsrPhase::Decay:
            apply_update(false, true, 0);
            if (level_ <= p_.sustain_level) {
                phase_ = AdsrPhase::Sustain;
                counter_ = 0;
            }
            break;
        case AdsrPhase::Sustain:
            apply_update(p_.sustain_inc, p_.sustain_exp,
                         p_.sustain_inc ? kAdsrMax : 0);
            break;
    }
    if (phase_ == AdsrPhase::Release) {
        apply_update(false, p_.release_exp, 0);
        if (level_ == 0) phase_ = AdsrPhase::Off;
    }
    return level_;
}
//@LABS-STUB
// TODO(9): drive the phase machine.
//   key_on(): load params, clear level/counter, enter Attack.
//   key_off(): enter Release unless already Off.
//   tick(): advance the phase's rate counter; once per `period` samples
//     apply one update (linear/exp rules from the header comment), then
//     transition Attack->Decay at 32767, Decay->Sustain at the sustain
//     level, Release->Off at 0. Return the current level.
inline void Adsr::key_on(const AdsrParams&) {
    // TODO(9): begin the attack phase.
}

inline void Adsr::key_off() {
    // TODO(9): begin the release phase.
}

inline int Adsr::tick() {
    // TODO(9): run the envelope state machine one sample forward.
    return 0;
}
//@LABS-END

}  // namespace spu
