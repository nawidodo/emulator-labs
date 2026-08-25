/* int_model_tests.c — pinning tests for the chapter 2 seed lab.
   Pure C17: plain asserts, one main, exit code drives ctest. */
#include <assert.h>
#include <stdio.h>

#include "int_model.h"

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
    /* endian reads */
    {
        const uint8_t d[4] = {0x34, 0x12, 0x78, 0x56};
        CHECK(c17_read_le16(d) == 0x1234);
        CHECK(c17_read_be16(d) == 0x3412);
        CHECK(c17_read_le32(d) == 0x56781234u);
    }

    /* sign extension */
    CHECK(c17_sign_extend_8(0x7F) == 127);
    CHECK(c17_sign_extend_8(0x80) == -128);
    CHECK(c17_sign_extend_8(0xFF) == -1);
    CHECK(c17_sign_extend_16(0x7FFF) == 32767);
    CHECK(c17_sign_extend_16(0x8000) == -32768);

    /* wrapping add + carry */
    {
        int c = -1;
        CHECK(c17_add8(0x01, 0x02, &c) == 0x03);
        CHECK(c == 0);
        CHECK(c17_add8(0xFF, 0x01, &c) == 0x00);
        CHECK(c == 1);
        CHECK(c17_add8(0xFF, 0xFF, &c) == 0xFE);
        CHECK(c == 1);
    }

    /* arithmetic shift right: floor division, sign-preserving */
    CHECK(c17_asr8((int8_t)64, 0) == 64);
    CHECK(c17_asr8((int8_t)64, 2) == 16);
    {
        const int8_t neg = (int8_t)0x90;   /* -112 */
        CHECK(c17_asr8(neg, 1) == -56);    /* floor(-56.0) */
        CHECK(c17_asr8(neg, 4) == -7);     /* floor(-7.0) */
        CHECK(c17_asr8((int8_t)-7, 1) == -4);   /* floor(-3.5) */
    }

    printf("c17_ch002: %d checks passed\n", checks);
    return 0;
}
