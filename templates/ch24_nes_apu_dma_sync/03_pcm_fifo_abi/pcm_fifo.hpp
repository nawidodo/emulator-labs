// pcm_fifo.hpp — host/audio boundary for the NES APU (foundations Phase 6,
// "Audio FIFO ABI"): the mixer produces deterministic PCM; the HOST owns
// playback. The core exposes a fixed-width, standard-layout contract that
// any language can mirror.
#pragma once

#include <cstddef>
#include <cstdint>

namespace nes24abi {

enum : int {
    PCM_OK = 0,
    PCM_ERR_VERSION = 1,
    PCM_ERR_SIZE = 2,
    PCM_ERR_NO_FIFO = 3,
    PCM_ERR_ARG = 4,
};

constexpr uint32_t kPcmAbiVersion = 1;

struct PcmFifoConfig {
    uint32_t struct_size;        // sizeof(PcmFifoConfig)
    uint32_t abi_version;        // kPcmAbiVersion
    uint32_t capacity_samples;   // ring depth in stereo sample pairs
};

struct PcmSample {
    int16_t left;
    int16_t right;
};

static_assert(sizeof(PcmFifoConfig) == 12, "config layout is ABI");
static_assert(sizeof(PcmSample) == 4, "sample layout is ABI");

struct PcmFifo;
PcmFifo* pcm_create(const PcmFifoConfig* cfg, int* out_err);
void pcm_destroy(PcmFifo* f);

// Push up to count samples; *out_accepted receives how many fit. Partial
// acceptance is normal when the ring is nearly full (host back-pressure).
int pcm_push(PcmFifo* f, const PcmSample* samples, uint32_t count,
             uint32_t* out_accepted);

// Pop up to `want` samples in FIFO order; *out_got receives how many were
// actually drained (0 when empty).
int pcm_pop(PcmFifo* f, PcmSample* out, uint32_t want, uint32_t* out_got);

int pcm_level(const PcmFifo* f, uint32_t* out_level);

}  // namespace nes24abi
