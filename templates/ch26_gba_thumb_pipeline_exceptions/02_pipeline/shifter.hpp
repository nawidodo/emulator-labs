#pragma once
#include <cstdint>

namespace arm {

// Minimal barrel shifter for Thumb format-1 immediate shifts.
// Self-contained copy of the ch25 shifter (LSL/LSR/ASR paths only — Thumb
// has no ROR/RRX in its instruction encodings).
struct ShiftResult {
    uint32_t value;
    bool carry_out;
};

enum ShiftType : uint32_t { kLSL = 0, kLSR = 1, kASR = 2 };

inline ShiftResult shift_imm(uint32_t type, uint32_t imm5, uint32_t rm,
                             bool c_in) {
    switch (type) {
    case kLSL: {
        if (imm5 == 0) return {rm, c_in};          // no-op: carry passes
        return {rm << imm5, ((rm >> (32 - imm5)) & 1) != 0};
    }
    case kLSR: {
        const uint32_t n = imm5 ? imm5 : 32;       // imm5==0 encodes #32
        if (n >= 32) return {0, (rm >> 31) != 0};
        return {rm >> n, ((rm >> (n - 1)) & 1) != 0};
    }
    default: {                                     // kASR
        const uint32_t n = imm5 ? imm5 : 32;
        if (n == 0) return {rm, c_in};
        if (n >= 32) {
            const bool sign = (rm >> 31) != 0;
            return {sign ? 0xFFFFFFFFu : 0, sign};
        }
        // Arithmetic right shift: replicate the sign bit.
        const bool sign = (rm >> 31) != 0;
        uint32_t v = rm >> n;
        if (sign)
            v |= (n >= 32) ? 0xFFFFFFFFu : (~0u << (32 - n));
        return {v, ((rm >> (n - 1)) & 1) != 0};
    }
    }
}

}  // namespace arm
