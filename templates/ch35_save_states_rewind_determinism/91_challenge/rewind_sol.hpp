// Vendored reference rewind (ch35/03 solution) for reuse.
#pragma once
// Rewind machinery: run-length codec + fixed-capacity ring of compressed
// states + step-back API. The caller captures every N frames; the ring
// keeps the most recent `capacity` snapshots (oldest are overwritten).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace snap {

// RLE: pairs of (count, byte), count 1..255. Runs of zeros (tile maps,
// cleared framebuffers) collapse dramatically.
inline std::vector<uint8_t> rle_compress(std::span<const uint8_t> in) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < in.size()) {
        const uint8_t v = in[i];
        size_t run = 1;
        while (run < 255 && i + run < in.size() && in[i + run] == v) ++run;
        out.push_back(uint8_t(run));
        out.push_back(v);
        i += run;
    }
    return out;
}

inline bool rle_decompress(std::span<const uint8_t> in,
                           std::vector<uint8_t>& out) {
    if (in.size() % 2 != 0) return false;
    out.clear();
    for (size_t i = 0; i < in.size(); i += 2) {
        const uint8_t count = in[i];
        if (count == 0) return false;
        out.insert(out.end(), count, in[i + 1]);
    }
    return true;
}

class Ring {
public:
    explicit Ring(size_t capacity)
        : slots_(capacity ? capacity : 1) {}

    // Compress and store; overwrites the oldest slot when full.
    void push(std::span<const uint8_t> state) {
        slots_[head_] = rle_compress(state);
        head_ = (head_ + 1) % slots_.size();
        if (count_ < slots_.size()) ++count_;
    }

    // n = how many captures back (0 = newest). Returns std::nullopt when
    // n is beyond what was captured.
    std::optional<std::vector<uint8_t>> step_back(size_t n) const {
        if (n >= count_) return std::nullopt;
        const size_t idx =
            (head_ + slots_.size() - 1 - n) % slots_.size();
        std::vector<uint8_t> out;
        if (!rle_decompress(slots_[idx], out)) return std::nullopt;
        return out;
    }

    size_t available() const { return count_; }
    size_t capacity() const { return slots_.size(); }

private:
    std::vector<std::vector<uint8_t>> slots_;
    size_t head_ = 0;   // next write position
    size_t count_ = 0;  // valid entries
};

}  // namespace snap
