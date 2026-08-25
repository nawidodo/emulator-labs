#pragma once
#include <cstdint>

namespace arm {

enum ShiftType : uint32_t { kLSL = 0, kLSR = 1, kASR = 2, kROR = 3 };

// The barrel shifter produces a value AND its own carry-out. Keeping that
// carry separate from the ALU carry is the architectural point of ch25.
// When a shift performs no work at all (amount == 0), the incoming C flag
// passes through unchanged, exactly like hardware.
struct ShiftResult {
    uint32_t value;
    bool carry_out;
};

constexpr uint32_t kAllOnes = 0xFFFFFFFFu;

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline ShiftResult shift_lsl(uint32_t amount, uint32_t rm, bool c_in) {
    if (amount == 0) return {rm, c_in};
    if (amount < 32) return {rm << amount, ((rm >> (32 - amount)) & 1) != 0};
    if (amount == 32) return {0, (rm & 1) != 0};
    return {0, false};
}
//@LABS-STUB
inline ShiftResult shift_lsl(uint32_t amount, uint32_t rm, bool c_in) {
    // TODO(1): LSL. Carry = last bit shifted out; #32 -> 0 with bit0;
    // #0 -> value Rm with C unchanged.
    (void)amount; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline ShiftResult shift_lsr(uint32_t amount, uint32_t rm, bool c_in) {
    if (amount == 0) return {rm, c_in};
    if (amount < 32)
        return {rm >> amount, ((rm >> (amount - 1)) & 1) != 0};
    if (amount == 32) return {0, (rm >> 31) != 0};
    return {0, false};
}
//@LABS-STUB
inline ShiftResult shift_lsr(uint32_t amount, uint32_t rm, bool c_in) {
    // TODO(2): LSR. Carry = bit (amount-1); #32 -> 0 with bit31; #0 -> C unchanged.
    (void)amount; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline ShiftResult shift_asr(uint32_t amount, uint32_t rm, bool c_in) {
    const bool sign = rm >> 31;
    if (amount == 0) return {rm, c_in};
    if (amount < 32)
        return {static_cast<uint32_t>(static_cast<int32_t>(rm) >> amount),
                ((rm >> (amount - 1)) & 1) != 0};
    return {sign ? kAllOnes : 0, sign};  // >= 32: sign-replicated
}
//@LABS-STUB
inline ShiftResult shift_asr(uint32_t amount, uint32_t rm, bool c_in) {
    // TODO(3): ASR. Counts >= 32 replicate the sign bit into value and carry.
    (void)amount; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline ShiftResult shift_ror(uint32_t amount, uint32_t rm, bool c_in) {
    if (amount == 0) return {rm, c_in};
    const uint32_t n = amount % 32;
    if (n == 0) return {rm, (rm >> 31) != 0};  // exact multiple of 32
    return {(rm >> n) | (rm << (32 - n)), ((rm >> (n - 1)) & 1) != 0};
}
//@LABS-STUB
inline ShiftResult shift_ror(uint32_t amount, uint32_t rm, bool c_in) {
    // TODO(4): ROR. Amount wraps mod 32; a full 32-rotation keeps Rm,
    // carry = bit31.
    (void)amount; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

// RRX: rotate right one place *through* the C flag. Encoded as an immediate
// ROR field of %0000.
//@LABS-BEGIN 5
//@LABS-SOLUTION
inline ShiftResult shift_rrx(uint32_t rm, bool c_in) {
    return {(rm >> 1) | (c_in ? (1u << 31) : 0), (rm & 1) != 0};
}
//@LABS-STUB
inline ShiftResult shift_rrx(uint32_t rm, bool c_in) {
    // TODO(5): old C enters bit 31, old bit 0 leaves as the new carry.
    (void)rm; (void)c_in;
    return {rm, false};
}
//@LABS-END

// Immediate-operand shifter over the imm5 encoding field:
//   LSL #0 is a no-op; LSR/ASR imm5==0 encode a count of 32;
//   ROR imm5==0 encodes RRX.
//@LABS-BEGIN 6
//@LABS-SOLUTION
inline ShiftResult shift_imm(uint32_t type, uint32_t imm5, uint32_t rm,
                             bool c_in) {
    switch (type) {
    case kLSL: return shift_lsl(imm5, rm, c_in);
    case kLSR: return shift_lsr(imm5 ? imm5 : 32, rm, c_in);
    case kASR: return shift_asr(imm5 ? imm5 : 32, rm, c_in);
    default:   return imm5 ? shift_ror(imm5, rm, c_in) : shift_rrx(rm, c_in);
    }
}
//@LABS-STUB
inline ShiftResult shift_imm(uint32_t type, uint32_t imm5, uint32_t rm,
                             bool c_in) {
    // TODO(6): dispatch to the helpers, resolving the imm5==0 special cases
    // (LSR/ASR -> 32, ROR -> RRX).
    (void)type; (void)imm5; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

// Register-specified shifter: low byte of Rs, true counts 0..255.
// Register ROR with amount 0 means "rotate by exactly 32", never RRX.
//@LABS-BEGIN 7
//@LABS-SOLUTION
inline ShiftResult shift_reg(uint32_t type, uint32_t rs_val, uint32_t rm,
                             bool c_in) {
    const uint32_t amount = rs_val & 0xFF;
    switch (type) {
    case kLSL: return shift_lsl(amount, rm, c_in);
    case kLSR: return shift_lsr(amount, rm, c_in);
    case kASR: return shift_asr(amount, rm, c_in);
    default:   return shift_ror(amount ? amount : 32, rm, c_in);
    }
}
//@LABS-STUB
inline ShiftResult shift_reg(uint32_t type, uint32_t rs_val, uint32_t rm,
                             bool c_in) {
    // TODO(7): dispatch on rs_val & 0xFF. Register-ROR amount 0 rotates by
    // 32 (value unchanged, carry = bit31).
    (void)type; (void)rs_val; (void)rm; (void)c_in;
    return {rm, c_in};
}
//@LABS-END

}  // namespace arm
