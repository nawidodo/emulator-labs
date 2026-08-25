// int_model.h — C17 integer model for emulation (chapter 2 seed lab).
//
// Contract: exact-width types only, no platform headers, deterministic
// pure functions. Compile is verified as strict C17 (-std=c17 -pedantic).
#pragma once

#include <stdint.h>

/* Read little/big endian 16/32-bit values from a byte pointer. */
uint16_t c17_read_le16(const uint8_t *p);
uint16_t c17_read_be16(const uint8_t *p);
uint32_t c17_read_le32(const uint8_t *p);

/* Sign-extend the low 8 / 16 bits to a full-width signed value. */
int32_t c17_sign_extend_8(uint8_t v);
int32_t c17_sign_extend_16(uint16_t v);

/* Wrapping 8-bit add: returns sum modulo 256, sets *carry on bit-8. */
uint8_t c17_add8(uint8_t a, uint8_t b, int *carry);

/* Arithmetic shift right by n (0..7): sign-extension, defined behavior. */
int8_t c17_asr8(int8_t v, unsigned n);
