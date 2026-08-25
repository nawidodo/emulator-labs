#pragma once
//
// ch46 / 02_idct — fixed-point separable IDCT for one 8x8 block.
//
// Basis matrix (documented constants, scale 32):
//   M[x][u] = round( c(u) * cos((2x+1) * u * pi / 16) * 32 )
//   with c(0) = 1/sqrt(2), c(u>0) = 1
//
// Two passes, each an integer dot product:
//   pass 1 (rows of coefficients): t[y][x] = ( sum_u F[y][u]*M[x][u] ) >> 5
//   pass 2 (columns):              o[y][x] = ( sum_v t[v][x]*M[y][v]
//                                            + 64 ) >> 7
// Basis scale is 32 (=> 1024 across both passes) and the DCT definition
// carries a 1/4 factor, so the total correction is >>7; +64 implements
// round-to-nearest on the final pass (psx-spx rounding note). All
// arithmetic is exact int64; results are clamped to the unsigned byte
// range by the caller (color-convert stage).

#include <cmath>
#include <cstdint>

namespace mdec {

constexpr unsigned kBlockSize8 = 8;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Generated from round(c(u)*cos((2x+1)*u*pi/16)*32); committed verbatim
// so every platform produces bit-identical transforms.
constexpr int32_t kRawBasis[8][8] = {
        {  23,   31,   30,   27,   23,   18,   12,    6},
        {  23,   27,   12,   -6,  -23,  -31,  -30,  -18},
        {  23,   18,  -12,  -31,  -23,    6,   30,   27},
        {  23,    6,  -30,  -18,   23,   27,  -12,  -31},
        {  23,   -6,  -30,   18,   23,  -27,  -12,   31},
        {  23,  -18,  -12,   31,  -23,   -6,   30,  -27},
        {  23,  -27,   12,    6,  -23,   31,  -30,   18},
        {  23,  -31,   30,  -27,   23,  -18,   12,   -6},
};

struct Basis {
    int32_t m[8][8];
    constexpr Basis() : m{} {
        for (int x = 0; x < 8; ++x)
            for (int u = 0; u < 8; ++u)
                m[x][u] = kRawBasis[x][u];
    }
};

constexpr Basis kBasis{};
//@LABS-STUB
// TODO(1): the basis table must hold round(c(u)*cos((2x+1)*u*pi/16)*32).
// The stub ships an all-zero matrix — transforms produce nothing.
struct Basis {
    int32_t m[8][8]{};
};
inline constexpr Basis kBasis{};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void idct8x8(const int in[64], int out[64]) {
    int32_t t[64];

    // Pass 1: coefficient rows -> intermediate spatial rows.
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int64_t sum = 0;
            for (int u = 0; u < 8; ++u)
                sum += static_cast<int64_t>(in[y * 8 + u]) *
                       kBasis.m[x][u];
            t[y * 8 + x] = static_cast<int32_t>(sum >> 5);
        }
    }

    // Pass 2: columns, with round-to-nearest (+2048 before shift).
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int64_t sum = 0;
            for (int v = 0; v < 8; ++v)
                sum += static_cast<int64_t>(t[v * 8 + x]) *
                       kBasis.m[y][v];
            out[y * 8 + x] = static_cast<int32_t>((sum + 64) >> 7);
        }
    }
}
//@LABS-STUB
// TODO(2): implement the two-pass separable transform exactly as
// documented in the file header (pass 1 >>5, pass 2 with +2048 >>12).
void idct8x8(const int in[64], int out[64]) {
    (void)in; (void)out;
}
//@LABS-END

}  // namespace mdec
