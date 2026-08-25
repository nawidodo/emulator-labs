// audio_ring.hpp — deterministic s16le stereo sample sink.
//
// Fixed-capacity ring of interleaved little-endian int16 L,R pairs.
// Pushes beyond capacity discard the OLDEST samples (a live audio card
// behaves the same way); drainTo/copyOut read the buffered span
// sequentially and are non-destructive.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace gbaudio {

class S16RingBuffer {
public:
    explicit S16RingBuffer(size_t capacitySamples = 1u << 20)
        : buf_(capacitySamples) {}

    size_t size() const { return count_; }
    size_t capacity() const { return buf_.size(); }

    // Append n interleaved samples; overflow drops the oldest ones.
    void push(const int16_t* samples, size_t n) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        for (size_t i = 0; i < n; ++i) {
            if (count_ == buf_.size()) {  // full: retire the oldest
                head_ = (head_ + 1) % buf_.size();
                --count_;
            }
            buf_[(head_ + count_) % buf_.size()] = samples[i];
            ++count_;
        }
//@LABS-STUB
        // TODO(1): append samples, wrapping modulo capacity; when full,
        // advance head_ past the oldest samples to make room.
        (void)samples;
        (void)n;
//@LABS-END
    }

    void pushStereo(int16_t left, int16_t right) {
        const int16_t pair[2] = {left, right};
        push(pair, 2);
    }

    // Write every buffered sample to `out` as raw s16le bytes.
    // Returns false only when the stream cannot be written.
    bool drainTo(std::FILE* out) const {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if (!out) return false;
        for (size_t i = 0; i < count_; ++i) {
            const int16_t s = buf_[(head_ + i) % buf_.size()];
            const uint8_t bytes[2] = {static_cast<uint8_t>(s & 0xFF),
                                      static_cast<uint8_t>((s >> 8) & 0xFF)};
            if (std::fwrite(bytes, 1, 2, out) != 2) return false;
        }
        return true;
//@LABS-STUB
        // TODO(2): fwrite each buffered sample low-byte-first (s16le),
        // oldest to newest; return false when `out` is null or a write
        // fails.
        (void)out;
        return false;
//@LABS-END
    }

    // In-memory variant used by unit tests (same ordering as drainTo).
    void copyOut(int16_t* dst) const {
        for (size_t i = 0; i < count_; ++i)
            dst[i] = buf_[(head_ + i) % buf_.size()];
    }

private:
    std::vector<int16_t> buf_;
    size_t head_ = 0;
    size_t count_ = 0;
};

}  // namespace gbaudio
