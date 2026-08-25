#pragma once
#include <cstdint>

#include "flags.hpp"

// 8080 ALU stage.
//
// The reference solution keeps each operation as a pure
// "operands in -> {value, CY, AC} out" step. Flag calculation is separated
// from register write-back so the CPU core can decide what to commit
// (CMP discards the value but keeps the flags; DAA consumes AC).

namespace i8080 {

struct AluResult {
    uint8_t value;
    bool cy;
    bool ac;
};

inline AluResult alu_add(uint8_t a, uint8_t b, bool carry_in) {
    uint16_t sum = uint16_t(a) + b + (carry_in ? 1 : 0);
    return {uint8_t(sum),
            (sum > 0xFF),
            aux_carry_add(a, b, carry_in)};
}

// a - b - borrow_in, computed as a + ~b + !borrow exactly like the 8080's
// adder. CY=1 means BORROW (carry-out clear); AC comes from the same
// half-sum as addition on the complemented operand.
inline AluResult alu_sub(uint8_t a, uint8_t b, bool borrow_in) {
    AluResult r = alu_add(a, uint8_t(~b), !borrow_in);
    return {r.value, !r.cy, r.ac};
}

// AND: CY cleared. The 8080 has a famous quirk — AC is set to the OR of
// bit 3 of both operands (not forced to 1 like the 8085). Diagnostics that
// run right before DAA depend on this exact behavior.
inline AluResult alu_ana(uint8_t a, uint8_t v) {
    uint8_t res = a & v;
    bool ac = ((a | v) & 0x08) != 0;
    return {res, false, ac};
}

// OR and XOR both clear CY and AC on the 8080 — the standard idiom for
// zeroing flags is `XRA A`.
inline AluResult alu_ora(uint8_t a, uint8_t v) {
    return {uint8_t(a | v), false, false};
}

inline AluResult alu_xra(uint8_t a, uint8_t v) {
    return {uint8_t(a ^ v), false, false};
}

}  // namespace i8080
