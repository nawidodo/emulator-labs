/* ptr_arrays.c — implementations for the chapter 3 seed lab. */
#include "ptr_arrays.h"

/* Length of a NUL-terminated string (excludes the terminating NUL). */
//@LABS-BEGIN 1
//@LABS-SOLUTION
size_t c17_bytes_until_nul(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}
//@LABS-STUB
/* TODO(1): walk s until the terminating NUL. */
size_t c17_bytes_until_nul(const char *s) {
    (void)s;
    return 0; /* wrong on purpose */
}
//@LABS-END

/* Concatenate a then b into out; -1 if out_cap too small (out unchanged). */
//@LABS-BEGIN 2
//@LABS-SOLUTION
int c17_join_words(const int *a, const int *b, size_t na, size_t nb,
                   int *out, size_t out_cap) {
    const size_t total = na + nb;
    if (total > out_cap) return -1;
    for (size_t i = 0; i < na; ++i) out[i] = a[i];
    for (size_t i = 0; i < nb; ++i) out[na + i] = b[i];
    return (int)total;
}
//@LABS-STUB
/* TODO(2): copy both arrays; -1 if out_cap is too small. */
int c17_join_words(const int *a, const int *b, size_t na, size_t nb,
                   int *out, size_t out_cap) {
    (void)a;
    (void)b;
    (void)na;
    (void)nb;
    (void)out;
    (void)out_cap;
    return -1; /* wrong on purpose */
}
//@LABS-END

/* Index of the maximum element; -1 if n == 0. */
//@LABS-BEGIN 3
//@LABS-SOLUTION
int c17_find_max(const int *a, size_t n) {
    if (n == 0) return -1;
    size_t best = 0;
    for (size_t i = 1; i < n; ++i)
        if (a[i] > a[best]) best = i;
    return (int)best;
}
//@LABS-STUB
/* TODO(3): return the index of the maximum element. */
int c17_find_max(const int *a, size_t n) {
    (void)a;
    (void)n;
    return -1; /* wrong on purpose */
}
//@LABS-END

/* Set all four fields of *r to the given value. */
//@LABS-BEGIN 4
//@LABS-SOLUTION
void c17_fill_rect(struct Rect *r, int color) {
    r->x = color;
    r->y = color;
    r->w = color;
    r->h = color;
}
//@LABS-STUB
/* TODO(4): assign color to every field of *r. */
void c17_fill_rect(struct Rect *r, int color) {
    (void)r;
    (void)color;
}
//@LABS-END

/* Area of the intersection of two rects; 0 if they do not overlap. */
//@LABS-BEGIN 5
//@LABS-SOLUTION
int c17_overlap_area(const struct Rect *a, const struct Rect *b) {
    const int x0 = a->x > b->x ? a->x : b->x;
    const int y0 = a->y > b->y ? a->y : b->y;
    const int x1 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
    const int y1 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w <= 0 || h <= 0) return 0;
    return w * h;
}
//@LABS-STUB
/* TODO(5): compute the area of the overlapping region. */
int c17_overlap_area(const struct Rect *a, const struct Rect *b) {
    (void)a;
    (void)b;
    return 0; /* wrong on purpose */
}
//@LABS-END

/* Accumulate w[0..n): checksum = (checksum + w[i]) * 31, starting from 0. */
//@LABS-BEGIN 6
//@LABS-SOLUTION
uint32_t c17_checksum_words(const uint16_t *w, size_t n) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < n; ++i)
        checksum = (checksum + w[i]) * 31u;
    return checksum;
}
//@LABS-STUB
/* TODO(6): checksum = (checksum + w[i]) * 31 over all elements. */
uint32_t c17_checksum_words(const uint16_t *w, size_t n) {
    (void)w;
    (void)n;
    return 0u; /* wrong on purpose */
}
//@LABS-END

/* Reverse the byte array p[0..n) in place. */
//@LABS-BEGIN 7
//@LABS-SOLUTION
void c17_reverse_bytes(uint8_t *p, size_t n) {
    for (size_t i = 0; i < n / 2; ++i) {
        const uint8_t t = p[i];
        p[i] = p[n - 1 - i];
        p[n - 1 - i] = t;
    }
}
//@LABS-STUB
/* TODO(7): reverse the byte order in place. */
void c17_reverse_bytes(uint8_t *p, size_t n) {
    (void)p;
    (void)n;
}
//@LABS-END

/* Write out[0]=r, out[1]=g, out[2]=b (demonstrates byte ownership). */
//@LABS-BEGIN 8
//@LABS-SOLUTION
void c17_pack_rgb(uint8_t *out, uint8_t r, uint8_t g, uint8_t b) {
    out[0] = r;
    out[1] = g;
    out[2] = b;
}
//@LABS-STUB
/* TODO(8): store r, g, b into out[0..2]. */
void c17_pack_rgb(uint8_t *out, uint8_t r, uint8_t g, uint8_t b) {
    (void)out;
    (void)r;
    (void)g;
    (void)b;
}
//@LABS-END