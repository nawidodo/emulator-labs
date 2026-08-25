#pragma once
// RewindSession: plays a CHIP-8 program with scripted input, capturing
// compressed states every kFramesPerCapture frames into a ring whose
// horizon is exactly 10 seconds (60 fps * 10 s / 10-frame quantum).
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "chip8.hpp"
#include "rewind_sol.hpp"    // vendored ch35/03 solution
#include "serialize.hpp"     // vendored ch35/01 solution

namespace challenge {

inline constexpr int kFps = 60;
inline constexpr int kRewindSeconds = 10;
inline constexpr int kFramesPerCapture = 10;
// 61 slots: 60 steps of 10 frames cover exactly 600 frames (10 s); the
// extra +1 slot is the landing pad so step_back(60) stays in range.
inline constexpr size_t kRingCapacity =
    size_t(kFps * kRewindSeconds / kFramesPerCapture) + 1;

class RewindSession {
public:
    explicit RewindSession(std::span<const uint8_t> rom) : ring_(kRingCapacity) {
        m_.load(rom);
        m_.reset();
    }

    // Advance one frame, capturing on the capture cadence.
    void advance() {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        m_.frame();
        if (++frames_since_capture_ == kFramesPerCapture) {
            std::vector<uint8_t> blob(chip8::kStateSize);
            chip8::write_state(m_, blob);
            ring_.push(blob);
            frames_since_capture_ = 0;
        }
//@LABS-STUB
        // TODO(1): run one machine frame; every kFramesPerCapture frames,
        // write_state into a blob and push it into ring_, then reset the
        // counter.
//@LABS-END
    }

    // Rewind by `seconds`, landing on the capture grid.
    bool rewind_seconds(int seconds) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        const size_t back =
            size_t(seconds) * (kRingCapacity / size_t(kRewindSeconds));
        const auto blob = ring_.step_back(back);
        if (!blob) return false;
        return chip8::read_state(*blob, m_);
//@LABS-STUB
        // TODO(2): step_back by the right number of captures for
        // `seconds` (kRingCapacity / kRewindSeconds per second) and
        // read_state the result. Fail cleanly when the ring has no such
        // history yet.
        (void)seconds;
        return true;  // wrong on purpose: rewinds nothing
//@LABS-END
    }

    uint64_t state_hash() const { return chip8::state_hash(m_); }
    const chip8::Machine& machine() const { return m_; }
    size_t history_depth() const { return ring_.available(); }

private:
    chip8::Machine m_;
    snap::Ring ring_;
    int frames_since_capture_ = 0;
};

}  // namespace challenge
