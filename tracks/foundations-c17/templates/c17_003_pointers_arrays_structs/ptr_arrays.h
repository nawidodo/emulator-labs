// ptr_arrays.h — C17 pointers, arrays, structs, and ownership (chapter 3
// seed lab).
//
// Contract: exact-width types only, no platform headers, deterministic
// pure functions. All routines operate on caller-owned buffers and never
// allocate. Compile is verified as strict C17 (-std=c17 -pedantic).
#pragma once

#include <stdint.h>
#include <stddef.h>

/* Axis-aligned rectangle with integer coordinates. */
struct Rect {
    int x, y, w, h;
};

/* Length of a NUL-terminated string (excludes the terminating NUL). */
size_t c17_bytes_until_nul(const char *s);

/* Concatenate a[0..na) then b[0..nb) into out[0..out_cap).
   Returns the total element count, or -1 if out_cap is too small
   (out is left unchanged in that case). */
int c17_join_words(const int *a, const int *b, size_t na, size_t nb,
                   int *out, size_t out_cap);

/* Index of the maximum element of a[0..n); -1 if n == 0.
   On ties, returns the first (lowest) index. */
int c17_find_max(const int *a, size_t n);

/* Set all four fields of *r to the given color-derived values.
   x = color, y = color, w = color, h = color. */
void c17_fill_rect(struct Rect *r, int color);

/* Area of the intersection of two rects; 0 if they do not overlap. */
int c17_overlap_area(const struct Rect *a, const struct Rect *b);

/* Accumulate w[0..n): checksum = (checksum + w[i]) * 31, starting from 0. */
uint32_t c17_checksum_words(const uint16_t *w, size_t n);

/* Reverse the byte array p[0..n) in place. */
void c17_reverse_bytes(uint8_t *p, size_t n);

/* Write out[0]=r, out[1]=g, out[2]=b (demonstrates byte ownership). */
void c17_pack_rgb(uint8_t *out, uint8_t r, uint8_t g, uint8_t b);
