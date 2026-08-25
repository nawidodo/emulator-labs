#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_adpcm.hpp"
#include "debug_adsr.hpp"

using namespace spu;

static std::array<uint8_t, 16> make_block(unsigned shift, unsigned filt,
                                          const unsigned* nibs) {
    std::array<uint8_t, 16> b{};
    b[0] = static_cast<uint8_t>((filt << 4) | (shift & 0xF));
    for (int i = 0; i < 28; ++i) {
        if (i % 2 == 0)
            b[2 + i / 2] |= static_cast<uint8_t>(nibs[i] & 0xF);
        else
            b[2 + i / 2] |= static_cast<uint8_t>((nibs[i] & 0xF) << 4);
    }
    return b;
}

TEST(debug_adpcm, history_delay_matches_reference) {
    // Block A ends with raw 4 (earlier nibbles zero), leaving hist1=4,
    // hist2=0 at the boundary. Block B (filter 2: c1=115, c2=-52) must
    // predict (4*115 + 0*-52)>>6 = 7.
    unsigned a[28] = {0};
    a[27] = 4;
    const unsigned bb[28] = {0};
    auto ba = make_block(0, 1, a);
    auto bc = make_block(0, 2, bb);

    DebugDecoder dec;
    dec.reset();
    dec.first_sample(ba.data());
    const int16_t dbg_b = dec.first_sample(bc.data());
    EXPECT_EQ(dbg_b, 7);

    // And the reference decoder agrees.
    AdpcmDecoder ref;
    ref.reset();
    ref.decode_block(ba.data());
    DecodedBlock out = ref.decode_block(bc.data());
    EXPECT_EQ(out.samples[0], 7);
}

TEST(debug_adpcm, whole_stream_identical_to_reference) {
    const unsigned n1[28] = {3, 5, 7, 9};
    const unsigned n2[28] = {1, 2, 4, 8};
    auto b1 = make_block(0, 2, n1);
    auto b2 = make_block(0, 3, n2);

    DebugDecoder dbg;
    dbg.reset();
    AdpcmDecoder ref;
    ref.reset();

    const int16_t d1 = dbg.first_sample(b1.data());
    DecodedBlock r1 = ref.decode_block(b1.data());
    EXPECT_EQ(d1, r1.samples[0]);

    const int16_t d2 = dbg.first_sample(b2.data());
    DecodedBlock r2 = ref.decode_block(b2.data());
    EXPECT_EQ(d2, r2.samples[0]);
}

TEST(debug_adpcm, reset_restores_reference_behaviour) {
    unsigned a[28] = {0};
    a[27] = 6;
    const unsigned bb[28] = {0};
    auto ba = make_block(0, 1, a);
    auto bc = make_block(0, 2, bb);

    DebugDecoder dbg;
    dbg.reset();
    dbg.first_sample(ba.data());
    dbg.reset();
    dbg.first_sample(ba.data());  // same history as the first attempt
    const int16_t again = dbg.first_sample(bc.data());

    AdpcmDecoder ref;
    ref.reset();
    ref.decode_block(ba.data());
    ref.reset();
    ref.decode_block(ba.data());
    DecodedBlock out = ref.decode_block(bc.data());
    EXPECT_EQ(again, out.samples[0]);
}

TEST(debug_adsr, exponential_decay_exact_quantum) {
    // From full scale with step 7: delta = (32767*7)>>6 = 3583 -> 29184.
    EXPECT_EQ(exp_decay_update(kAdsrMax, 7), 29184);
}
