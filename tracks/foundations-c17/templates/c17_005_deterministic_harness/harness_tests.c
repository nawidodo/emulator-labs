/* harness_tests.c — pinning tests for the chapter 5 seed lab.
   Pure C17: plain checks, one main, exit code drives ctest.
   Identical in solution and skeleton trees (no @LABS markers). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "harness.h"

static int checks = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                    #cond);                                                \
            return 1;                                                      \
        }                                                                  \
        ++checks;                                                          \
    } while (0)

/* Helpers for run_suite tests */
static bool passing_test(void) { return true; }
static bool failing_test(void) { return false; }

int main(void) {
    /* fnv1a64: known vectors */
    {
        /* empty input -> offset basis */
        uint64_t h0 = c17_fnv1a64((const uint8_t *)"", 0);
        CHECK(h0 == 0xCBF29CE484222325ULL);
        /* "abc" -> 0xe71fa2190541574b (FNV-1a 64) */
        const uint8_t abc[] = {'a', 'b', 'c'};
        uint64_t habc = c17_fnv1a64(abc, 3);
        CHECK(habc == 0xe71fa2190541574bULL);
        /* single byte "a" */
        const uint8_t a[] = {'a'};
        uint64_t ha = c17_fnv1a64(a, 1);
        CHECK(ha == 0xaf63dc4c8601ec8cULL);
        /* deterministic: same input same hash */
        CHECK(c17_fnv1a64(abc, 3) == habc);
    }

    /* hexdump */
    {
        char out[32];
        const uint8_t data1[] = {0x00, 0xFF, 0xAB};
        c17_hexdump(data1, 3, out);
        CHECK(strcmp(out, "00ffab") == 0);
        /* empty */
        c17_hexdump(data1, 0, out);
        CHECK(strcmp(out, "") == 0);
        /* single byte */
        const uint8_t one[] = {0x0f};
        c17_hexdump(one, 1, out);
        CHECK(strcmp(out, "0f") == 0);
        /* full range */
        const uint8_t all[] = {0xDE, 0xAD, 0xBE, 0xEF};
        c17_hexdump(all, 4, out);
        CHECK(strcmp(out, "deadbeef") == 0);
    }

    /* expect_eq_u32 */
    {
        CHECK(c17_expect_eq_u32(42u, 42u, "42u==42u", __FILE__, __LINE__) == true);
        CHECK(c17_expect_eq_u32(1u, 2u, "1u==2u", __FILE__, __LINE__) == false);
        CHECK(c17_expect_eq_u32(0u, 0u, "0==0", __FILE__, __LINE__) == true);
        CHECK(c17_expect_eq_u32(0xFFFFFFFFu, 0xFFFFFFFFu, "max==max", __FILE__, __LINE__) == true);
        CHECK(c17_expect_eq_u32(0xFFFFFFFFu, 0u, "max!=0", __FILE__, __LINE__) == false);
    }

    /* expect_mem_eq */
    {
        const uint8_t aa[] = {1, 2, 3};
        const uint8_t bb[] = {1, 2, 3};
        const uint8_t cc[] = {1, 2, 4};
        CHECK(c17_expect_mem_eq(aa, bb, 3, "aa==bb", __FILE__, __LINE__) == true);
        CHECK(c17_expect_mem_eq(aa, cc, 3, "aa!=cc", __FILE__, __LINE__) == false);
        CHECK(c17_expect_mem_eq(aa, bb, 0, "empty", __FILE__, __LINE__) == true);
        const uint8_t dd[] = {0xAA};
        const uint8_t ee[] = {0xBB};
        CHECK(c17_expect_mem_eq(dd, ee, 1, "aa!=bb single", __FILE__, __LINE__) == false);
        CHECK(c17_expect_mem_eq(dd, dd, 1, "same", __FILE__, __LINE__) == true);
    }

    /* run_suite */
    {
        c17_test_t passing[] = {{"pass1", passing_test}, {"pass2", passing_test}};
        CHECK(c17_run_suite(passing, 2) == 0);
        c17_test_t mixed[] = {{"pass", passing_test}, {"fail", failing_test}};
        CHECK(c17_run_suite(mixed, 2) == 1);
        c17_test_t single_fail[] = {{"fail", failing_test}};
        CHECK(c17_run_suite(single_fail, 1) == 1);
        /* empty suite should succeed */
        CHECK(c17_run_suite(NULL, 0) == 0);
    }

    printf("c17_ch005: %d checks passed\n", checks);
    return 0;
}
