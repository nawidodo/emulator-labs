#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>
#include <cstddef>

#include "challenge_gpu.hpp"
#include "fixture_quad.hpp"

using namespace psx::gpu;

// The challenge acceptance criterion: feeding the committed GP0 command
// stream (fill + CLUT/texture uploads + two textured triangles forming a
// quad + textured rectangles) through the reference pipeline reproduces the
// golden VRAM hash exactly.
TEST(challenge, quad_stream_matches_golden_vram) {
    auto dev_storage = std::make_unique<GpuDevice<ChallengeStages>>();
    GpuDevice<ChallengeStages>& dev = *dev_storage;
    for (size_t i = 0; i < ch42fixture::kQuadWordCount; ++i)
        dev.feed(ch42fixture::kQuadStream[i], static_cast<uint32_t>(i * 4));
    EXPECT_EQ(fnv64_vram(dev.vram), ch42fixture::kQuadVramHash);
}

// Determinism guard: running the same stream twice must be byte-identical.
TEST(challenge, render_is_deterministic) {
    auto a_storage = std::make_unique<GpuDevice<ChallengeStages>>();
    GpuDevice<ChallengeStages>& a = *a_storage;
    auto b_storage = std::make_unique<GpuDevice<ChallengeStages>>();
    GpuDevice<ChallengeStages>& b = *b_storage;
    for (size_t i = 0; i < ch42fixture::kQuadWordCount; ++i) {
        a.feed(ch42fixture::kQuadStream[i], 0);
        b.feed(ch42fixture::kQuadStream[i], 0);
    }
    EXPECT_EQ(fnv64_vram(a.vram), fnv64_vram(b.vram));
}
