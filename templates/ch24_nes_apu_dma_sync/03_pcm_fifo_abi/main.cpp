#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "pcm_fifo.hpp"

using namespace nes24abi;

namespace {

constexpr uint32_t kCap = 8;

PcmFifo* make_fifo(int* err) {
    PcmFifoConfig c{};
    c.struct_size = sizeof(PcmFifoConfig);
    c.abi_version = kPcmAbiVersion;
    c.capacity_samples = kCap;
    return pcm_create(&c, err);
}

PcmSample sample(int16_t v) { return PcmSample{v, static_cast<int16_t>(-v)}; }

}  // namespace

TEST(fifo, create_validates_abi_fields) {
    PcmFifoConfig c{};
    int err = -1;
    EXPECT_EQ(pcm_create(&c, &err), nullptr);
    EXPECT_EQ(err, PCM_ERR_SIZE);          // struct_size left 0

    c.struct_size = sizeof(PcmFifoConfig);
    err = -1;
    EXPECT_EQ(pcm_create(&c, &err), nullptr);
    EXPECT_EQ(err, PCM_ERR_VERSION);       // version left 0

    c.abi_version = kPcmAbiVersion;
    err = -1;
    EXPECT_EQ(pcm_create(&c, &err), nullptr);
    EXPECT_EQ(err, PCM_ERR_ARG);           // zero capacity

    c.capacity_samples = kCap;
    err = -1;
    PcmFifo* f = pcm_create(&c, &err);
    EXPECT_EQ(err, PCM_OK);
    EXPECT_NE(f, nullptr);
    pcm_destroy(f);
}

TEST(fifo, push_pop_preserves_order_across_wrap) {
    int err = 0;
    PcmFifo* f = make_fifo(&err);

    // Push 12 samples into an 8-deep ring: only 8 accepted.
    std::vector<PcmSample> in;
    for (int16_t i = 0; i < 12; ++i) in.push_back(sample(i));
    uint32_t accepted = 99;
    EXPECT_EQ(pcm_push(f, in.data(), 12, &accepted), PCM_OK);
    EXPECT_EQ(accepted, kCap);

    // Drain all 8: exact FIFO order must survive the wrap.
    std::vector<PcmSample> out(kCap, PcmSample{0, 0});
    uint32_t got = 0;
    EXPECT_EQ(pcm_pop(f, out.data(), kCap, &got), PCM_OK);
    EXPECT_EQ(got, kCap);
    for (uint32_t i = 0; i < kCap; ++i) {
        EXPECT_EQ(out[i].left, static_cast<int16_t>(i));
        EXPECT_EQ(out[i].right, static_cast<int16_t>(-i));
    }

    // Empty now: pop reports zero drained.
    got = 77;
    EXPECT_EQ(pcm_pop(f, out.data(), 4, &got), PCM_OK);
    EXPECT_EQ(got, 0u);                    // stub invents samples here
    pcm_destroy(f);
}

TEST(fifo, partial_accept_reports_backpressure) {
    int err = 0;
    PcmFifo* f = make_fifo(&err);
    std::vector<PcmSample> six(6, sample(1));
    uint32_t accepted = 0;
    pcm_push(f, six.data(), 6, &accepted);
    EXPECT_EQ(accepted, 6u);

    // Room for only 2 more.
    std::vector<PcmSample> five(5, sample(9));
    accepted = 99;
    pcm_push(f, five.data(), 5, &accepted);
    EXPECT_EQ(accepted, 2u);               // stub reports 0

    uint32_t lvl = 0;
    EXPECT_EQ(pcm_level(f, &lvl), PCM_OK);
    EXPECT_EQ(lvl, kCap);
    pcm_destroy(f);
}
