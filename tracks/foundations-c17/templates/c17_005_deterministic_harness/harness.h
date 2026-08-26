// harness.h — C17 deterministic test harness (chapter 5 seed lab).
//
// Contract: exact-width types only, no platform headers, deterministic
// pure functions. All routines operate on caller-owned buffers and never
// allocate. Compile is verified as strict C17 (-std=c17 -pedantic).
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char *name;
    bool (*fn)(void);
} c17_test_t;

/* Run each test in tests[0..n), printing "[ RUN ] name" / "[ OK ]" /
   "[ FAIL ]". Returns 0 if all pass, 1 otherwise. */
int c17_run_suite(const c17_test_t *tests, size_t n);

/* Expect a == b; on mismatch prints FAIL file:line: expr (a != b) and
   returns false, otherwise returns true. */
bool c17_expect_eq_u32(uint32_t a, uint32_t b, const char *expr,
                       const char *file, int line);

/* Expect a[0..n) == b[0..n); on mismatch prints FAIL and returns false,
   otherwise returns true. */
bool c17_expect_mem_eq(const uint8_t *a, const uint8_t *b, size_t n,
                       const char *expr, const char *file, int line);

/* FNV-1a 64-bit hash (offset 0xCBF29CE484222325, prime 0x100000001B3). */
uint64_t c17_fnv1a64(const uint8_t *p, size_t n);

/* Hex-dump p[0..n) into out[0..2*n] as lower-case hex, NUL-terminated.
   out must have space for 2*n+1 bytes. */
void c17_hexdump(const uint8_t *p, size_t n, char *out);
