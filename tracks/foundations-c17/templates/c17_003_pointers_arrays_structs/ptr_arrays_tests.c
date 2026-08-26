/* ptr_arrays_tests.c — pinning tests for the chapter 3 seed lab.
   Pure C17: plain checks, one main, exit code drives ctest.
   Identical in solution and skeleton trees (no @LABS markers). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ptr_arrays.h"

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
    /* NUL-terminated string length */
    CHECK(c17_bytes_until_nul("abc") == 3);
    CHECK(c17_bytes_until_nul("") == 0);
    CHECK(c17_bytes_until_nul("hello, world") == 12);

    /* join words: exact fit and too-small out_cap */
    {
        const int a[3] = {1, 2, 3};
        const int b[2] = {4, 5};
        int out[5];
        memset(out, 0xFF, sizeof out);
        CHECK(c17_join_words(a, b, 3, 2, out, 5) == 5);
        CHECK(out[0] == 1 && out[1] == 2 && out[2] == 3);
        CHECK(out[3] == 4 && out[4] == 5);
    }
    {
        const int a[3] = {1, 2, 3};
        const int b[2] = {4, 5};
        int out[4];
        memset(out, 0x00, sizeof out);
        CHECK(c17_join_words(a, b, 3, 2, out, 4) == -1);
        CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);
        CHECK(out[3] == 0);   /* out must be left unchanged */
    }

    /* find max */
    {
        const int x[3] = {3, 7, 2};
        CHECK(c17_find_max(x, 3) == 1);
        CHECK(c17_find_max(x, 0) == -1);
        CHECK(c17_find_max(x, 1) == 0);
    }

    /* fill rect sets all fields */
    {
        struct Rect r = {0, 0, 0, 0};
        c17_fill_rect(&r, 42);
        CHECK(r.x == 42 && r.y == 42 && r.w == 42 && r.h == 42);
    }

    /* overlap area: disjoint -> 0, overlapping -> computed area */
    {
        const struct Rect a = {0, 0, 10, 10};
        const struct Rect b = {20, 20, 5, 5};
        CHECK(c17_overlap_area(&a, &b) == 0);
    }
    {
        const struct Rect a = {0, 0, 10, 10};
        const struct Rect b = {5, 5, 10, 10};
        CHECK(c17_overlap_area(&a, &b) == 25);  /* 5 x 5 overlap */
    }
    {
        /* just touching edges is an empty overlap */
        const struct Rect a = {0, 0, 10, 10};
        const struct Rect b = {10, 0, 5, 5};
        CHECK(c17_overlap_area(&a, &b) == 0);
    }

    /* checksum words: known answer */
    {
        /* ((0+1)*31+2)*31+3)*31 = 1023*31+3*31 = 31806 */
        const uint16_t w[3] = {1, 2, 3};
        CHECK(c17_checksum_words(w, 3) == 31806u);
        CHECK(c17_checksum_words(w, 0) == 0u);
    }

    /* reverse bytes in place */
    {
        uint8_t b[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
        c17_reverse_bytes(b, 5);
        CHECK(b[0] == 0x05 && b[1] == 0x04 && b[2] == 0x03);
        CHECK(b[3] == 0x02 && b[4] == 0x01);
    }

    /* pack rgb fills bytes */
    {
        uint8_t out[3] = {0, 0, 0};
        c17_pack_rgb(out, 0xAA, 0xBB, 0xCC);
        CHECK(out[0] == 0xAA && out[1] == 0xBB && out[2] == 0xCC);
    }

    printf("c17_ch003: %d checks passed\n", checks);
    return 0;
}