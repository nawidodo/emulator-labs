/* int_model.c — implementations for the chapter 2 seed lab. */
#include "int_model.h"

//@LABS-BEGIN 1
//@LABS-SOLUTION
uint16_t c17_read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
//@LABS-STUB
/* TODO(1): read little-endian 16-bit (low byte first). */
uint16_t c17_read_le16(const uint8_t *p) {
    (void)p;
    return 0; /* wrong on purpose */
}
//@LABS-END

uint32_t c17_read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t c17_read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

//@LABS-BEGIN 2
//@LABS-SOLUTION
int32_t c17_sign_extend_8(uint8_t v) {
    union { uint8_t u; int8_t s; } cvt;
    cvt.u = v;
    return cvt.s;
}
//@LABS-STUB
/* TODO(2): sign-extend bit 7 into the full 32-bit result. */
int32_t c17_sign_extend_8(uint8_t v) {
    (void)v;
    return 0; /* wrong on purpose */
}
//@LABS-END

int32_t c17_sign_extend_16(uint16_t v) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    union { uint16_t u; int16_t s; } cvt;
    cvt.u = v;
    return cvt.s;
//@LABS-STUB
    /* TODO(3): sign-extend bit 15 into the full 32-bit result. */
    (void)v;
    return 0; /* wrong on purpose */
//@LABS-END
}

uint8_t c17_add8(uint8_t a, uint8_t b, int *carry) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    const uint16_t sum = (uint16_t)a + (uint16_t)b;
    if (carry) *carry = (sum >> 8) & 1;
    return (uint8_t)(sum & 0xFFu);
//@LABS-STUB
    /* TODO(4): wrapping add; report bit-8 carry through *carry. */
    if (carry) *carry = 0;
    (void)a;
    (void)b;
    return 0; /* wrong on purpose */
//@LABS-END
}

int8_t c17_asr8(int8_t v, unsigned n) {
    /* Arithmetic shift right = floor division by 2^n, implemented with
       fully defined operations (no reliance on >> for negatives). */
    if (n >= 7) return (v < 0) ? (int8_t)-1 : (int8_t)0;
    const int32_t wide = v;
    const int32_t denom = 1 << n;
    int32_t q = wide / denom;
    if (wide < 0 && wide % denom != 0) --q;   /* floor, not truncation */
    return (int8_t)q;
}
