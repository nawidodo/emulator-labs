#define LABSTEST_MAIN
#include "labstest.hpp"
#include "fixture.hpp"
#include "golden.hpp"
#include "../05_mix/spu.hpp"
#include "../shared/fnv.hpp"
#include <cstddef>

using namespace spu;

// Challenge: play the bundled jingle through a full voice pipeline
// (decode -> key_on -> pitch -> ADSR -> mix) and pin the rendered PCM.
//
// Setup mirrors the public runner script tests/public/ch47_ps1_spu/
// scripts/jingle.script: ROM at byte 0x1000, voice 0 at word 0x200,
// pitch 0x1000, fast linear attack/release, unity volumes.
static std::vector<int16_t> render_jingle() {
    Spu spu;
    spu.reset();
    const auto rom = jingle_rom();
    spu.dma_write(0x1000,
                  std::span<const uint8_t>(rom.data(), rom.size()));
    spu.write(0x000, 0x4000);
    spu.write(0x002, 0x4000);
    spu.write(0x004, 0x1000);
    spu.write(0x006, 0x0200);          // start_addr >>3 -> byte 0x1000
    // ar=64 lin attack; sr=64 lin inc sustain; rr=64 lin release
    spu.write(0x008, static_cast<uint16_t>((64 << 8) | (15 << 4) | 1));
    spu.write(0x00A, static_cast<uint16_t>((1 << 13) | (64 << 6) | 64));
    spu.write(0x180, 0x4000);
    spu.write(0x182, 0x4000);
    spu.write(0x1C0, 0x0001);

    std::vector<int16_t> pcm;
    spu.render(4000, pcm);
    return pcm;
}

TEST(challenge_jingle, decodes_all_four_blocks) {
    AdpcmDecoder dec;
    dec.reset();
    const auto rom = jingle_rom();
    int blocks = 0;
    for (unsigned off = 0; off < rom.size(); off += 16) {
        auto out = dec.decode_block(&rom[off]);
        ++blocks;
        if (off == 0) {
            EXPECT_TRUE(out.flags.loop_start);
            EXPECT_EQ(out.samples[0], sign_nibble((rom[2]) & 0xF) << 3);
        }
        if (off == rom.size() - 16) EXPECT_TRUE(out.flags.loop_end);
    }
    EXPECT_EQ(blocks, 4);
}

TEST(challenge_jingle, pcm_matches_public_golden) {
    const auto pcm = render_jingle();
    EXPECT_EQ(pcm.size(), size_t(8000));
    const auto* bytes = reinterpret_cast<const uint8_t*>(pcm.data());
    EXPECT_EQ(fnv64({bytes, pcm.size() * 2}),
              kGoldenJinglePcmFnv64);
}
