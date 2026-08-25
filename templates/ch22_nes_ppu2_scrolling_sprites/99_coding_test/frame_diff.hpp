#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
// ch22 coding test — screenshot-diff diagnoser.
//
// Given two raw RGBA8 frames (256x240), report:
//   hash_a / hash_b : FNV-1a 64 of each frame's bytes
//   ndiff           : number of differing pixels
//   first=x,y       : first differing pixel in scan order
//   shift=          : h1 if B is A scrolled one pixel horizontally,
//                     v1 if vertically, other otherwise
//
// Classification rule (documented, deterministic): B is "h1-scrolled" when
// every pixel where B differs from A satisfies B(x,y) == A(x-1,y) or
// B(x,y) == A(x+1,y) — i.e., all differences are explained by a 1-pixel
// horizontal displacement. Same idea vertically for v1. Anything else is
// "other". Frames are compared only inside the 256x240 active area.
namespace nes22diff {

constexpr int kW = 256;
constexpr int kH = 240;

inline uint64_t fnv1a64(const uint8_t* data, size_t n) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

struct DiffReport {
    uint64_t hash_a = 0;
    uint64_t hash_b = 0;
    int ndiff = 0;
    int first_x = -1;
    int first_y = -1;
    const char* shift = "other";
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void count_diff(const std::string& a_str, const std::string& b_str,
                       DiffReport& r) {
    const uint8_t* a = reinterpret_cast<const uint8_t*>(a_str.data());
    const uint8_t* b = reinterpret_cast<const uint8_t*>(b_str.data());
    r.hash_a = fnv1a64(a, size_t(kW) * kH * 4);
    r.hash_b = fnv1a64(b, size_t(kW) * kH * 4);
    r.ndiff = 0;
    r.first_x = -1;
    r.first_y = -1;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            size_t o = (size_t(y) * kW + x) * 4;
            bool same = a[o] == b[o] && a[o + 1] == b[o + 1]
                        && a[o + 2] == b[o + 2];
            if (!same) {
                if (r.first_x < 0) {
                    r.first_x = x;
                    r.first_y = y;
                }
                ++r.ndiff;
            }
        }
    }
}
//@LABS-STUB
// TODO(1): fill hash_a/hash_b (fnv1a64 over 256*240*4 bytes), then scan in
// scan order counting differing pixels (compare RGB; ignore alpha) and
// record the FIRST difference coordinates. Stub leaves everything zero.
inline void count_diff(const std::string& /*a*/, const std::string& /*b*/,
                       DiffReport& /*r*/) {}
//@LABS-END

// True when B(x,y) equals the source pixel of A at horizontal offset d.
inline bool pixel_matches_shifted_h(const uint8_t* a, const uint8_t* b,
                                    int x, int y, int d) {
    int sx = x - d;
    if (sx < 0 || sx >= kW) return false;
    size_t ob = (size_t(y) * kW + x) * 4;
    size_t oa = (size_t(y) * kW + sx) * 4;
    return a[oa] == b[ob] && a[oa + 1] == b[ob + 1]
           && a[oa + 2] == b[ob + 2];
}

inline bool pixel_matches_shifted_v(const uint8_t* a, const uint8_t* b,
                                    int x, int y, int d) {
    int sy = y - d;
    if (sy < 0 || sy >= kH) return false;
    size_t ob = (size_t(y) * kW + x) * 4;
    size_t oa = (size_t(sy) * kW + x) * 4;
    return a[oa] == b[ob] && a[oa + 1] == b[ob + 1]
           && a[oa + 2] == b[ob + 2];
}

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void classify_shift(const std::string& a_str, const std::string& b_str,
                           DiffReport& r) {
    if (r.ndiff == 0) {
        r.shift = "none";
        return;
    }
    const uint8_t* a = reinterpret_cast<const uint8_t*>(a_str.data());
    const uint8_t* b = reinterpret_cast<const uint8_t*>(b_str.data());

    auto all_explained = [&](auto shifted) {
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x) {
                size_t o = (size_t(y) * kW + x) * 4;
                bool same = a[o] == b[o] && a[o + 1] == b[o + 1]
                            && a[o + 2] == b[o + 2];
                if (same) continue;
                if (!shifted(x, y, 1) && !shifted(x, y, -1)) return false;
            }
        return true;
    };
    if (all_explained([&](int x, int y, int d) {
            return pixel_matches_shifted_h(a, b, x, y, d);
        }))
        r.shift = "h1";
    else if (all_explained([&](int x, int y, int d) {
                 return pixel_matches_shifted_v(a, b, x, y, d);
             }))
        r.shift = "v1";
    else
        r.shift = "other";
}
//@LABS-STUB
// TODO(2): set r.shift to "none" when ndiff==0; otherwise "h1" when EVERY
// differing pixel of B matches A shifted by +-1 column, "v1" for +-1 row
// (use pixel_matches_shifted_* helpers), else "other". Stub says "other".
inline void classify_shift(const std::string& /*a*/, const std::string& /*b*/,
                           DiffReport& /*r*/) {}
//@LABS-END

inline std::string report_text(const DiffReport& r) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "hash_a=%016llX\nhash_b=%016llX\nndiff=%d\nfirst=%d,%d\n"
                  "shift=%s\n",
                  (unsigned long long)r.hash_a,
                  (unsigned long long)r.hash_b, r.ndiff, r.first_x,
                  r.first_y, r.shift);
    return buf;
}

}  // namespace nes22diff
