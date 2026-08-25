#pragma once
#include <cstdint>

#include "../01_adpcm/adpcm.hpp"

namespace spu {

// Debugging target #1: multi-block ADPCM streams decode with a subtly
// wrong timbre. This helper decodes a whole block exactly like the
// reference (so both sides walk the same 28 samples) and returns the
// block's first sample for easy comparison.
class DebugDecoder {
public:
    void reset();
    int16_t first_sample(const uint8_t* b);

private:
    int32_t hist1_ = 0;
    int32_t hist2_ = 0;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void DebugDecoder::reset() {
    hist1_ = 0;
    hist2_ = 0;
}

inline int16_t DebugDecoder::first_sample(const uint8_t* b) {
    const unsigned shift = b[0] & 0xF;
    const unsigned filt = b[0] >> 4 > 4 ? 4u : b[0] >> 4;
    const int32_t c1 = FILTER_C[filt][0];
    const int32_t c2 = FILTER_C[filt][1];
    int16_t first = 0;
    for (int i = 0; i < 28; ++i) {
        const uint8_t byte = b[2 + i / 2];
        const unsigned nib = (i % 2 == 0) ? (byte & 0xF) : (byte >> 4);
        int32_t s = sign_nibble(nib) << shift;
        s += (hist1_ * c1 + hist2_ * c2) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        // History shifts down: hist2 must receive the OLD hist1.
        hist2_ = hist1_;
        hist1_ = s;
        if (i == 0) first = static_cast<int16_t>(s);
    }
    return first;
}
//@LABS-STUB
// TODO(1): this decode path carries a SEEDED BUG. Filtered streams
// (filters 2-4) come out metallic; filter 0/1 blocks start correctly.
// Compare against ../01_adpcm, find the defect in the predictor history,
// fix it, then write bug-report.md.
inline void DebugDecoder::reset() {
    hist1_ = 0;
    hist2_ = 0;
}

inline int16_t DebugDecoder::first_sample(const uint8_t* b) {
    const unsigned shift = b[0] & 0xF;
    const unsigned filt = b[0] >> 4 > 4 ? 4u : b[0] >> 4;
    const int32_t c1 = FILTER_C[filt][0];
    const int32_t c2 = FILTER_C[filt][1];
    int16_t first = 0;
    for (int i = 0; i < 28; ++i) {
        const uint8_t byte = b[2 + i / 2];
        const unsigned nib = (i % 2 == 0) ? (byte & 0xF) : (byte >> 4);
        int32_t s = sign_nibble(nib) << shift;
        s += (hist1_ * c1 + hist2_ * c2) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        // Seeded bug: do not change anything except what you diagnose.
        hist1_ = s;
        hist2_ = s;
        if (i == 0) first = static_cast<int16_t>(s);
    }
    return first;
}
//@LABS-END

}  // namespace spu
