#pragma once
#include <array>
#include <cstdint>

namespace spu {

// ---------------------------------------------------------------------------
// PS1 ADPCM (the format every SPU sample and CD-XA audio block uses).
//
// A block is 16 bytes:
//
//   byte 0 : low nibble  = shift (0..15)
//            high nibble = filter (0..4; 5..7 are reserved on real hardware,
//                          we clamp to 4, which is what most emulators do)
//   byte 1 : flags
//              bit 0: loop end   - last block of the sound / of the loop
//              bit 1: mute       - silence the voice after this block
//              bit 2: loop start - marks the address the loop jumps back to
//   bytes 2..15 : 14 data bytes -> 28 nibbles -> 28 samples.
//
// Nibble order is LOW nibble first within each data byte. Each 4-bit value
// is sign-extended, scaled by the shift, then run through a 2-tap IIR
// predictor whose coefficients come from the published filter table below
// (PSX-SPX "CD-ROM ADPCM"). The predictor history carries across blocks
// until reset().
// ---------------------------------------------------------------------------

// Published coefficient table, index = filter nibble: {c1, c2}.
// The prediction is  s = raw + ((hist1*c1 + hist2*c2) >> 6).
extern const int8_t FILTER_C[5][2];

struct BlockFlags {
    bool loop_end = false;
    bool mute = false;
    bool loop_start = false;
};

struct DecodedBlock {
    std::array<int16_t, 28> samples{};
    BlockFlags flags{};
};

class AdpcmDecoder {
public:
    void reset();

    // Decodes one 16-byte block into 28 PCM samples.
    DecodedBlock decode_block(const uint8_t* b);

private:
    int32_t hist1_ = 0;
    int32_t hist2_ = 0;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Sign-extend a 4-bit value: nibble 0xF -> -1, 0x8 -> -8, 0x7 -> 7.
inline int32_t sign_nibble(unsigned n) {
    return static_cast<int32_t>(static_cast<int16_t>((n & 0xF) << 12) >> 12);
}
//@LABS-STUB
// TODO(1): sign-extend a 4-bit value (nibble 0xF must become -1,
// 0x8 -> -8, 0x7 -> +7). Return 0 for now so the suite compiles RED.
inline int32_t sign_nibble(unsigned n) {
    (void)n;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline DecodedBlock AdpcmDecoder::decode_block(const uint8_t* b) {
    DecodedBlock out;
    const unsigned shift = b[0] & 0xF;
    const unsigned filt = b[0] >> 4 > 4 ? 4u : b[0] >> 4;
    const int32_t c1 = FILTER_C[filt][0];
    const int32_t c2 = FILTER_C[filt][1];

    for (int i = 0; i < 28; ++i) {
        // Low nibble first within each byte; data starts at byte 2.
        const uint8_t byte = b[2 + i / 2];
        const unsigned nib = (i % 2 == 0) ? (byte & 0xF) : (byte >> 4);
        int32_t s = sign_nibble(nib) << shift;
        // Predictor taps use the OLD history, then history shifts down.
        s += (hist1_ * c1 + hist2_ * c2) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out.samples[i] = static_cast<int16_t>(s);
        hist2_ = hist1_;
        hist1_ = s;
    }
    out.flags.loop_end = b[1] & 0x1;
    out.flags.mute = b[1] & 0x2;
    out.flags.loop_start = b[1] & 0x4;
    return out;
}

inline void AdpcmDecoder::reset() {
    hist1_ = 0;
    hist2_ = 0;
}
//@LABS-STUB
inline void AdpcmDecoder::reset() {
}

inline DecodedBlock AdpcmDecoder::decode_block(const uint8_t*) {
    // TODO(2): implement the block decode described above; this stub
    // returns silence so the suite runs RED until you finish it.
    return {};
}
//@LABS-END

}  // namespace spu
