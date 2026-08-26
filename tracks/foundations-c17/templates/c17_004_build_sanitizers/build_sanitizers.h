// build_sanitizers.h — C17 build system, assertions, and sanitizers (chapter 4
// seed lab).
//
// Contract: exact-width types only, no platform headers, deterministic
// pure functions. All routines operate on caller-owned buffers and never
// allocate. Compile is verified as strict C17 (-std=c17 -pedantic).
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* True iff x is a power of two (exactly one bit set). */
bool c17_is_power_of_two(uint32_t x);

/* Clamp v into [lo, hi]. */
int c17_clamp(int v, int lo, int hi);

/* Checked add: 0 on success (writes *out), -1 on overflow. */
int c17_checked_add(int a, int b, int *out);

/* Round n up to the next multiple of align (power of two).
   Returns 0 if align is 0 or not a power of two. */
size_t c17_align_up(size_t n, size_t align);

/* Parse decimal string s as uint8_t in 0..255.
   Returns 0 on success (writes *out), -1 on error. */
int c17_parse_u8(const char *s, uint8_t *out);

/* Fill dst[0..n) with value v. */
void c17_fill_u32(uint32_t *dst, size_t n, uint32_t v);

/* Compare a[0..n) and b[0..n): 0 if equal, 1 if differ. */
int c17_mem_eq(const uint8_t *a, const uint8_t *b, size_t n);

/* Accumulate p[0..n): checksum = (checksum + p[i]) * 31, starting from 0. */
uint32_t c17_packet_checksum(const uint8_t *p, size_t n);
