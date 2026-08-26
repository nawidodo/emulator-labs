/* build_sanitizers_tests.c — pinning tests for the chapter 4 seed lab.
   Pure C17: plain checks, one main, exit code drives ctest.
   Identical in solution and skeleton trees (no @LABS markers). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "build_sanitizers.h"

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

int main(void) {
    /* is_power_of_two */
    CHECK(c17_is_power_of_two(1) == true);
    CHECK(c17_is_power_of_two(2) == true);
    CHECK(c17_is_power_of_two(4) == true);
    CHECK(c17_is_power_of_two(16) == true);
    CHECK(c17_is_power_of_two(3) == false);
    CHECK(c17_is_power_of_two(0) == false);
    CHECK(c17_is_power_of_two(6) == false);
    CHECK(c17_is_power_of_two(1u << 31) == true);

    /* clamp */
    CHECK(c17_clamp(5, 0, 3) == 3);
    CHECK(c17_clamp(-1, 0, 10) == 0);
    CHECK(c17_clamp(5, 0, 10) == 5);
    CHECK(c17_clamp(0, 0, 10) == 0);
    CHECK(c17_clamp(10, 0, 10) == 10);
    CHECK(c17_clamp(15, 0, 10) == 10);

    /* checked_add */
    {
        int out = 0;
        CHECK(c17_checked_add(2, 3, &out) == 0 && out == 5);
        CHECK(c17_checked_add(-5, 5, &out) == 0 && out == 0);
        CHECK(c17_checked_add(INT_MAX, 1, &out) == -1);
        CHECK(c17_checked_add(INT_MIN, -1, &out) == -1);
        CHECK(c17_checked_add(INT_MAX, 0, &out) == 0 && out == INT_MAX);
    }

    /* align_up */
    CHECK(c17_align_up(5, 8) == 8);
    CHECK(c17_align_up(8, 8) == 8);
    CHECK(c17_align_up(0, 8) == 0);
    CHECK(c17_align_up(9, 8) == 16);
    CHECK(c17_align_up(5, 0) == 0);
    CHECK(c17_align_up(5, 3) == 0);
    CHECK(c17_align_up(5, 6) == 0);
    CHECK(c17_align_up(16, 4) == 16);
    CHECK(c17_align_up(17, 4) == 20);

    /* parse_u8 */
    {
        uint8_t v = 0xFF;
        CHECK(c17_parse_u8("255", &v) == 0 && v == 255);
        CHECK(c17_parse_u8("0", &v) == 0 && v == 0);
        CHECK(c17_parse_u8("42", &v) == 0 && v == 42);
        CHECK(c17_parse_u8("256", &v) == -1);
        CHECK(c17_parse_u8("abc", &v) == -1);
        CHECK(c17_parse_u8("", &v) == -1);
        CHECK(c17_parse_u8("-1", &v) == -1);
        CHECK(c17_parse_u8(" 5", &v) == -1);
        CHECK(c17_parse_u8("12x", &v) == -1);
    }

    /* fill_u32 */
    {
        uint32_t dst[4] = {0, 0, 0, 0};
        c17_fill_u32(dst, 4, 0xDEADBEEFu);
        CHECK(dst[0] == 0xDEADBEEFu && dst[1] == 0xDEADBEEFu);
        CHECK(dst[2] == 0xDEADBEEFu && dst[3] == 0xDEADBEEFu);
        c17_fill_u32(dst, 0, 0x12345678u);
        CHECK(dst[0] == 0xDEADBEEFu);
    }

    /* mem_eq */
    {
        const uint8_t a[3] = {1, 2, 3};
        const uint8_t b[3] = {1, 2, 3};
        const uint8_t c[3] = {1, 2, 4};
        CHECK(c17_mem_eq(a, b, 3) == 0);
        CHECK(c17_mem_eq(a, c, 3) == 1);
        CHECK(c17_mem_eq(a, b, 0) == 0);
        CHECK(c17_mem_eq(a, c, 1) == 0);
        CHECK(c17_mem_eq(a, c, 2) == 0);
    }

    /* packet_checksum: known answer */
    {
        const uint8_t p[3] = {1, 2, 3};
        /* ((0+1)*31+2)*31+3)*31 = 31806 */
        CHECK(c17_packet_checksum(p, 3) == 31806u);
        CHECK(c17_packet_checksum(p, 0) == 0u);
        const uint8_t q[4] = {0xFF, 0x00, 0x01, 0x02};
        uint32_t exp = 0u;
        for (size_t i = 0; i < 4; ++i) exp = (exp + q[i]) * 31u;
        CHECK(c17_packet_checksum(q, 4) == exp);
    }

    printf("c17_ch004: %d checks passed\n", checks);
    return 0;
}
