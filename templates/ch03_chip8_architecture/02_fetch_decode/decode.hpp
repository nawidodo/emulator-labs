#pragma once
// decode.hpp — pure opcode field extraction (TODO3).
//
// Every CHIP-8 instruction is one 16-bit big-endian word. The field layout,
// using 6XNN as the example:
//
//     0110 XXXX NNNN NNNN
//      op   X    NN
//
// These helpers are constexpr and side-effect free so they can be exhaustively
// unit-tested without a machine.
#include <cstdint>

namespace chip8::decode {

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Big-endian fetch: mem[pc] is the high byte, mem[pc+1] the low byte.
inline constexpr uint16_t fetch_opcode(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) << 8 | p[1]);
}
//@LABS-STUB
inline constexpr uint16_t fetch_opcode(const uint8_t* p) {
    // TODO(1): combine the two bytes big-endian (high byte first).
    (void)p;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// NNN: low 12 bits — an address (JP/CALL/LD I target).
inline constexpr uint16_t nnn(uint16_t op) { return op & 0x0FFF; }
// N: lowest nibble — usually a sprite height or 4-bit immediate.
inline constexpr uint8_t n(uint16_t op) { return op & 0xF; }
// NN: low byte — 8-bit immediate.
inline constexpr uint8_t nn(uint16_t op) { return op & 0xFF; }
//@LABS-STUB
inline constexpr uint16_t nnn(uint16_t op) {
    // TODO(2): extract the low 12 bits.
    (void)op;
    return 0;
}
inline constexpr uint8_t n(uint16_t op) {
    // TODO(2): extract the lowest nibble.
    (void)op;
    return 0;
}
inline constexpr uint8_t nn(uint16_t op) {
    // TODO(2): extract the low byte.
    (void)op;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// X selects VX — bits 11..8 of the opcode. Getting this from any
// other nibble is the classic first CHIP-8 decoder bug.
inline constexpr uint8_t x(uint16_t op) { return (op >> 8) & 0xF; }
// Y selects VY — bits 7..4 of the opcode.
inline constexpr uint8_t y(uint16_t op) { return (op >> 4) & 0xF; }
//@LABS-STUB
inline constexpr uint8_t x(uint16_t op) {
    // TODO(3): extract the register selector X (bits 11..8 of the opcode).
    (void)op;
    return 0;
}
inline constexpr uint8_t y(uint16_t op) {
    // TODO(3): extract the register selector Y (low nibble of high byte).
    (void)op;
    return 0;
}
//@LABS-END

}  // namespace chip8::decode
