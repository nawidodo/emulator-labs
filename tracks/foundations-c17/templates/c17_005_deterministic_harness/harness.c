/* harness.c — implementations for the chapter 5 seed lab. */
#include "harness.h"

#include <stdio.h>
#include <string.h>

//@LABS-BEGIN 1
//@LABS-SOLUTION
int c17_run_suite(const c17_test_t *tests, size_t n) {
    size_t fails = 0;
    for (size_t i = 0; i < n; ++i) {
        const char *name = tests[i].name ? tests[i].name : "(null)";
        printf("[ RUN ] %s\n", name);
        bool ok = false;
        if (tests[i].fn) {
            ok = tests[i].fn();
        }
        if (ok) {
            printf("[ OK ] %s\n", name);
        } else {
            printf("[ FAIL ] %s\n", name);
            ++fails;
        }
    }
    return fails == 0 ? 0 : 1;
}
//@LABS-STUB
/* TODO(1): loop over tests, print [ RUN ] / [ OK ] / [ FAIL ], return 0 if all pass else 1. */
int c17_run_suite(const c17_test_t *tests, size_t n) {
    (void)tests;
    (void)n;
    return 1; /* wrong on purpose */
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
bool c17_expect_eq_u32(uint32_t a, uint32_t b, const char *expr,
                       const char *file, int line) {
    if (a == b) {
        return true;
    }
    fprintf(stderr, "FAIL %s:%d: %s (%u != %u)\n", file, line, expr, a, b);
    return false;
}
//@LABS-STUB
/* TODO(2): return a==b, printing FAIL file:line: expr (a != b) on mismatch. */
bool c17_expect_eq_u32(uint32_t a, uint32_t b, const char *expr,
                       const char *file, int line) {
    (void)a;
    (void)b;
    (void)expr;
    (void)file;
    (void)line;
    return false; /* wrong on purpose */
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
bool c17_expect_mem_eq(const uint8_t *a, const uint8_t *b, size_t n,
                       const char *expr, const char *file, int line) {
    if (n == 0) {
        return true;
    }
    if (a == NULL || b == NULL) {
        fprintf(stderr, "FAIL %s:%d: %s (null pointer with n=%zu)\n", file, line, expr, n);
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            fprintf(stderr, "FAIL %s:%d: %s (mismatch at %zu: 0x%02x != 0x%02x)\n",
                    file, line, expr, i, a[i], b[i]);
            return false;
        }
    }
    return true;
}
//@LABS-STUB
/* TODO(3): compare a[0..n) and b[0..n), print FAIL on mismatch, return equality. */
bool c17_expect_mem_eq(const uint8_t *a, const uint8_t *b, size_t n,
                       const char *expr, const char *file, int line) {
    (void)a;
    (void)b;
    (void)n;
    (void)expr;
    (void)file;
    (void)line;
    return false; /* wrong on purpose */
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
uint64_t c17_fnv1a64(const uint8_t *p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ULL;
    const uint64_t prime = 0x100000001B3ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)p[i];
        h *= prime;
    }
    return h;
}
//@LABS-STUB
/* TODO(4): FNV-1a 64-bit with offset 0xCBF29CE484222325 and prime 0x100000001B3. */
uint64_t c17_fnv1a64(const uint8_t *p, size_t n) {
    (void)p;
    (void)n;
    return 0ULL; /* wrong on purpose */
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
void c17_hexdump(const uint8_t *p, size_t n, char *out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = hex[(p[i] >> 4) & 0xF];
        out[2 * i + 1] = hex[p[i] & 0xF];
    }
    out[2 * n] = '\0';
}
//@LABS-STUB
/* TODO(5): write p[0..n) as lower-case hex into out, NUL-terminated. */
void c17_hexdump(const uint8_t *p, size_t n, char *out) {
    (void)p;
    (void)n;
    (void)out;
    if (out) out[0] = '\0';
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
static char c17_nibble_to_hex(unsigned v) {
    return "0123456789abcdef"[v & 0xF];
}
//@LABS-STUB
/* TODO(6): helper to convert a nibble to lower-case hex. */
static char c17_nibble_to_hex(unsigned v) {
    (void)v;
    return '?'; /* wrong on purpose */
}
//@LABS-END
