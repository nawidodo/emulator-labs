#pragma once
#include <cstdint>

// Golden constants for ch50_ps1_accuracy_trace_testing.
//
// EVERY value in this table was produced by the reference psx-mini
// implementation in shared/psx_mini.hpp (generate.py --mode solution) and
// regenerated TWICE; both runs were byte-identical before committing.
// Generation commands are recorded in
// tests/public/ch50_ps1_accuracy_trace_testing/provenance.md.
//
// These pins are intentionally independent of the ten seeded regressions in
// 90_debug: they exercise the suite infrastructure itself, so a student who
// has not touched 90_debug still sees the built-in checks go green.
namespace psxmini {

inline constexpr uint64_t kGoldenCpuTraceFnv64 = 0xCF308C9DFDC038BBULL;
inline constexpr uint32_t kGoldenCpuFinalR1 = 15;          // sum 1..5
inline constexpr uint64_t kGoldenVramFnv64 = 0xDB9D0A2CD0F05825ULL;
inline constexpr uint64_t kGoldenSpuFnv64 = 0x535D3882E62B2346ULL;

// DMA end-of-block pin (block of 8 words from madr 0x40 over the ramp RAM).
inline constexpr uint32_t kGoldenDmaMadr = 0x40 + 4 * 8;
inline constexpr uint64_t kGoldenDmaDevFnv64 = 0x41996165CA04C645ULL;

// GTE pin: identity-ish scale matrix applied to v = {4096, -2048, 1024}.
inline constexpr int16_t kGoldenGteIr[3] = {8704, -257, 1536};

// Timer pin after 400 sysclks with div=4, target=37.
inline constexpr unsigned kGoldenTimerReached = 2;
inline constexpr uint16_t kGoldenTimerCnt = 24;

// CDROM pin: read 6 sectors from LBA 2.
inline constexpr uint32_t kGoldenCdromLba = 8;
inline constexpr unsigned kGoldenCdromCount = 6;
inline constexpr uint64_t kGoldenCdromHashFnv64 = 0xC5F01DCC5CEC2222ULL;

// 91_challenge: FNV-1a 64 of the exact default-suite report text emitted by
// ch50_01_accuracy_runner (7 PASS lines plus the summary line).
inline constexpr uint64_t kGoldenSuiteSummaryFnv64 = 0xB25E44C705E9555AULL;

}  // namespace psxmini
