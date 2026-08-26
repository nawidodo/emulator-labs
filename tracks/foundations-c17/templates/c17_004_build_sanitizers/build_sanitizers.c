/* build_sanitizers.c — implementations for the chapter 4 seed lab. */
#include "build_sanitizers.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/* True iff x is a power of two (exactly one bit set). */
//@LABS-BEGIN 1
//@LABS-SOLUTION
bool c17_is_power_of_two(uint32_t x) {
    return x != 0u && (x & (x - 1u)) == 0u;
}
//@LABS-STUB
/* TODO(1): return true iff x is a power of two. */
bool c17_is_power_of_two(uint32_t x) {
    (void)x;
    return false; /* wrong on purpose */
}
//@LABS-END

/* Clamp v into [lo, hi]. */
//@LABS-BEGIN 2
//@LABS-SOLUTION
int c17_clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
//@LABS-STUB
/* TODO(2): clamp v into [lo, hi]. */
int c17_clamp(int v, int lo, int hi) {
    (void)lo;
    (void)hi;
    return v; /* wrong on purpose: no clamping */
}
//@LABS-END

/* Checked add: 0 on success (writes *out), -1 on overflow. */
//@LABS-BEGIN 3
//@LABS-SOLUTION
int c17_checked_add(int a, int b, int *out) {
    if (out == NULL) return -1;
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return -1;
    }
    *out = a + b;
    return 0;
}
//@LABS-STUB
/* TODO(3): checked add with overflow detection. */
int c17_checked_add(int a, int b, int *out) {
    (void)a;
    (void)b;
    (void)out;
    return -1; /* wrong on purpose */
}
//@LABS-END

/* Round n up to the next multiple of align (power of two).
   Returns 0 if align is 0 or not a power of two. */
//@LABS-BEGIN 4
//@LABS-SOLUTION
size_t c17_align_up(size_t n, size_t align) {
    if (align == 0u || (align & (align - 1u)) != 0u) {
        return 0;
    }
    assert(align && (align & (align-1))==0);
    return (n + align - 1u) & ~(align - 1u);
}
//@LABS-STUB
/* TODO(4): align n up to next multiple of power-of-two align. */
size_t c17_align_up(size_t n, size_t align) {
    (void)n;
    (void)align;
    return 0; /* wrong on purpose */
}
//@LABS-END

/* Parse decimal string s as uint8_t in 0..255.
   Returns 0 on success (writes *out), -1 on error. */
//@LABS-BEGIN 5
//@LABS-SOLUTION
int c17_parse_u8(const char *s, uint8_t *out) {
    if (s == NULL || out == NULL) return -1;
    if (s[0] == '\0') return -1;
    if (s[0] == '-' || s[0] == '+') return -1;
    if (s[0] < '0' || s[0] > '9') return -1;
    errno = 0;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    if (errno == ERANGE) return -1;
    if (v > 255ul) return -1;
    *out = (uint8_t)v;
    return 0;
}
//@LABS-STUB
/* TODO(5): parse decimal 0..255. */
int c17_parse_u8(const char *s, uint8_t *out) {
    (void)s;
    (void)out;
    return -1; /* wrong on purpose */
}
//@LABS-END

/* Fill dst[0..n) with value v. */
//@LABS-BEGIN 6
//@LABS-SOLUTION
void c17_fill_u32(uint32_t *dst, size_t n, uint32_t v) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = v;
    }
}
//@LABS-STUB
/* TODO(6): fill dst[0..n) with v. */
void c17_fill_u32(uint32_t *dst, size_t n, uint32_t v) {
    (void)dst;
    (void)n;
    (void)v;
}
//@LABS-END

/* Compare a[0..n) and b[0..n): 0 if equal, 1 if differ. */
//@LABS-BEGIN 7
//@LABS-SOLUTION
int c17_mem_eq(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}
//@LABS-STUB
/* TODO(7): compare memory regions. */
int c17_mem_eq(const uint8_t *a, const uint8_t *b, size_t n) {
    (void)a;
    (void)b;
    (void)n;
    return 0; /* wrong on purpose */
}
//@LABS-END

/* Accumulate p[0..n): checksum = (checksum + p[i]) * 31, starting from 0. */
//@LABS-BEGIN 8
//@LABS-SOLUTION
uint32_t c17_packet_checksum(const uint8_t *p, size_t n) {
    uint32_t checksum = 0u;
    for (size_t i = 0; i < n; ++i) {
        checksum = (checksum + p[i]) * 31u;
    }
    return checksum;
}
//@LABS-STUB
/* TODO(8): checksum = (checksum + p[i]) * 31 over all elements. */
uint32_t c17_packet_checksum(const uint8_t *p, size_t n) {
    (void)p;
    (void)n;
    return 0u; /* wrong on purpose */
}
//@LABS-END
