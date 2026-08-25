// audio_debug.hpp — a minimal sweep+envelope voice for the debugging
// drill. It models the parts of the pulse channel where the two seeded
// defects live; the skeleton carries the BUGS, the solution is correct.
//
// Committed model (identical to exercise 01):
//   * envelopeTick() at 64 Hz: period 0 freezes volume; the timer counts
//     down and on expiry reloads to `envPeriod` then steps the volume one
//     notch toward 15/0.
//   * sweepTick() at 128 Hz: applies the candidate computed one update
//     earlier (candidates below 0 discarded), refreshes shadow/freq, then
//     computes the next candidate from the FRESH shadow for BOTH modes
//     and disables the channel when that second candidate exceeds 2047.
#pragma once

#include <cstdint>

namespace gbapudbg {

class SweepEnvVoice {
public:
    // ---- configuration ----
    uint8_t pace = 0;
    uint8_t slope = 0;
    bool negate = false;
    uint8_t envPeriod = 0;
    uint8_t initialVolume = 0;
    bool envIncrease = false;

    // ---- state ----
    bool enabled = false;
    uint16_t freq = 0;
    uint16_t shadow = 0;
    int32_t pendingCandidate = 0;
    bool hasPendingCandidate = false;
    int volume = 0;

    int32_t calcCandidate() const {
        const int32_t delta = shadow >> slope;
        return negate ? static_cast<int32_t>(shadow) - delta
                      : static_cast<int32_t>(shadow) + delta;
    }

    void trigger() {
        enabled = true;
        volume = initialVolume;
        envTimer = envPeriod;
        if (pace != 0) {
            shadow = freq;
            sweepTimer = pace;
            hasPendingCandidate = false;
            const int32_t c = calcCandidate();
            if (c > 2047) {
                enabled = false;
            } else {
                pendingCandidate = c;
                hasPendingCandidate = true;
            }
        }
    }

    // Envelope tick (64 Hz).
    void envelopeTick() {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        if (envPeriod == 0) return;
        if (--envTimer == 0) {
            envTimer = envPeriod;
            if (envIncrease) {
                if (volume < 15) ++volume;
            } else if (volume > 0) {
                --volume;
            }
        }
//@LABS-STUB
        // TODO(1): this build decays too slowly — it burns one extra idle
        // tick after every reload before the volume actually moves, i.e.
        // an effective period+1 rate. Make the step happen exactly when
        // the countdown reaches zero.
        if (envPeriod == 0) return;
        if (envTimer > 0) {
            --envTimer;
            return;              // BUG: wastes the tick after each reload
        }
        envTimer = envPeriod;
        if (envIncrease) {
            if (volume < 15) ++volume;
        } else if (volume > 0) {
            --volume;
        }
//@LABS-END
    }

    // Sweep tick (128 Hz).
    void sweepTick() {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if (pace == 0 || !hasPendingCandidate) return;
        if (--sweepTimer > 0) return;
        sweepTimer = pace;
        const int32_t c = pendingCandidate;
        hasPendingCandidate = false;
        if (c > 2047) {
            enabled = false;
            return;
        }
        if (c >= 0) {  // negative candidates are discarded untouched
            shadow = static_cast<uint16_t>(c);
            freq = static_cast<uint16_t>(c);
        }
        pendingCandidate = calcCandidate();  // SECOND update, fresh shadow
        hasPendingCandidate = true;
        if (pendingCandidate > 2047) enabled = false;
//@LABS-STUB
        // TODO(2): this build misbehaves in NEGATIVE mode only: the
        // second calculation reads the stale pre-update shadow instead of
        // the freshly applied one, and its overflow-disable rule is
        // missing entirely. Restore the committed behavior.
        if (pace == 0 || !hasPendingCandidate) return;
        if (--sweepTimer > 0) return;
        sweepTimer = pace;
        int32_t c = pendingCandidate;
        hasPendingCandidate = false;
        if (c > 2047) {
            enabled = false;
            return;
        }
        const uint16_t preUpdate = shadow;   // BUG: captured before apply
        if (c >= 0) {
            shadow = static_cast<uint16_t>(c);
            freq = static_cast<uint16_t>(c);
        }
        if (!negate) {                       // positive path is correct
            pendingCandidate = calcCandidate();
            if (pendingCandidate > 2047) enabled = false;
        } else {
            const int32_t delta = preUpdate >> slope;
            pendingCandidate =
                static_cast<int32_t>(preUpdate) - delta;  // stale shadow
            // BUG: no overflow disable after this SECOND update
        }
        hasPendingCandidate = true;
//@LABS-END
    }

    int sample() const { return enabled ? volume : 0; }

private:
    int envTimer = 0;
    int sweepTimer = 0;
};

}  // namespace gbapudbg
