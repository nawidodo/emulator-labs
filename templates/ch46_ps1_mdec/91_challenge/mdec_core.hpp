#pragma once
//
// ch46 / 91_challenge — DMA-fed MDEC macroblock pipeline and frame hash.
//
// Stream byte format (big-endian), repeated until EOF:
//   per macroblock: six blocks in order Y0 Y1 Y2 Y3 Cb Cr;
//   per block: u16 nunits, then nunits x u16 (RLZ units, see rlz.hpp).
//
// DmaFeed models the channel-1 (MDECout) FIFO: the CPU/DMA pushes 32-bit
// words, the decoder drains 16-bit units — exactly like hardware.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <vector>

#include "../01_rlz/rlz.hpp"
#include "../02_idct/idct.hpp"
#include "../03_color_convert/color.hpp"

namespace mchal {

//@LABS-BEGIN 1
//@LABS-SOLUTION
class DmaFeed {
public:
    void push_word(uint32_t w) {
        units_.push_back(static_cast<uint16_t>(w >> 16));   // high half first
        units_.push_back(static_cast<uint16_t>(w & 0xFFFFu));
    }
    bool read_unit(uint16_t& u) {
        if (pos_ >= units_.size()) return false;
        u = units_[pos_++];
        return true;
    }
    size_t remaining() const { return units_.size() - pos_; }

private:
    std::vector<uint16_t> units_;
    size_t pos_ = 0;
};
//@LABS-STUB
// TODO(1): DMA words carry two RLZ units each: bits 31..16 first, then
// bits 15..0. read_unit drains them oldest-first.
class DmaFeed {
public:
    void push_word(uint32_t) {}
    bool read_unit(uint16_t&) { return false; }
    size_t remaining() const { return 0; }
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Decodes one 6-block macroblock from the feed into 256 RGB15 words.
inline bool decode_macroblock(DmaFeed& feed, uint16_t out[256]) {
    int blocks[6][64];
    for (unsigned b = 0; b < 6; ++b) {
        uint16_t n = 0;
        if (!feed.read_unit(n)) return false;   // block length prefix
        if (n == 0 || n > 4096) return false;

        // Collect this block's 16-bit units (re-padded into a scratch).
        std::vector<uint16_t> units(n);
        for (uint16_t i = 0; i < n; ++i)
            if (!feed.read_unit(units[i])) return false;

        int coeffs[mdec::kBlockSize];
        mdec::decode_block(units.data(), n, coeffs);
        mdec::idct8x8(coeffs, blocks[b]);
    }
    mdec::assemble_macroblock(blocks, out);
    return true;
}
//@LABS-STUB
// TODO(2): per block read the u16 length prefix then that many units,
// run decode_block + idct8x8; after all six blocks call
// assemble_macroblock. Return false on truncated/absurd input.
bool decode_macroblock(DmaFeed& feed, uint16_t out[256]) {
    (void)feed; (void)out;
    return false;
}
//@LABS-END

// FNV-1a 64 over raw output bytes (matches tools/labs/hash_frame.py).
inline uint64_t fnv1a64(const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

struct FrameResult {
    std::vector<uint16_t> pixels;  // 256 per macroblock, concatenated
    unsigned macroblocks = 0;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Decodes a whole stream file of concatenated macroblocks.
inline FrameResult decode_stream(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);

    DmaFeed feed;
    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    for (size_t i = 0; i + 3 < bytes.size(); i += 4) {
        uint32_t w = (uint32_t(bytes[i]) << 24) |
                     (uint32_t(bytes[i + 1]) << 16) |
                     (uint32_t(bytes[i + 2]) << 8) | bytes[i + 3];
        feed.push_word(w);
    }

    FrameResult res;
    uint16_t mb[256];
    while (feed.remaining() > 0) {
        if (!decode_macroblock(feed, mb))
            break;  // trailing garbage / truncation: stop cleanly
        res.pixels.insert(res.pixels.end(), mb, mb + 256);
        ++res.macroblocks;
    }
    return res;
}
//@LABS-STUB
// TODO(3): load the file as big-endian u32 DMA words into the feed, then
// decode_macroblock repeatedly until exhausted; append each 256-word
// macroblock to the result.
FrameResult decode_stream(const std::string& path) {
    (void)path;
    return {};
}
//@LABS-END

}  // namespace mchal
