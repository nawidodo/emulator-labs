// core.cpp — ring-buffer implementation behind the PCM ABI.
#include "pcm_fifo.hpp"
#include <cstring>
#include <new>
#include <vector>

namespace nes24abi {

struct PcmFifo {
    PcmFifoConfig cfg{};
    uint32_t head = 0;   // next write slot
    uint32_t tail = 0;   // next read slot
    uint32_t level = 0;  // occupied slots
    std::vector<PcmSample> ring;
};

PcmFifo* pcm_create(const PcmFifoConfig* cfg, int* out_err) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (cfg == nullptr || cfg->struct_size != sizeof(PcmFifoConfig)) {
        if (out_err) *out_err = PCM_ERR_SIZE;
        return nullptr;
    }
    if (cfg->abi_version != kPcmAbiVersion) {
        if (out_err) *out_err = PCM_ERR_VERSION;
        return nullptr;
    }
    if (cfg->capacity_samples == 0) {
        if (out_err) *out_err = PCM_ERR_ARG;
        return nullptr;
    }
    PcmFifo* f = new (std::nothrow) PcmFifo();
    if (!f) {
        if (out_err) *out_err = PCM_ERR_SIZE;
        return nullptr;
    }
    f->cfg = *cfg;
    f->ring.assign(cfg->capacity_samples, PcmSample{0, 0});
    if (out_err) *out_err = PCM_OK;
    return f;
//@LABS-STUB
    // TODO(1): validate struct_size / abi_version / nonzero capacity and
    // allocate a zeroed FIFO whose ring holds capacity_samples entries.
    (void)cfg;
    if (out_err) *out_err = PCM_ERR_VERSION;  // wrong on purpose
    return nullptr;
//@LABS-END
}

void pcm_destroy(PcmFifo* f) { delete f; }

int pcm_push(PcmFifo* f, const PcmSample* samples, uint32_t count,
             uint32_t* out_accepted) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    if (f == nullptr || (count > 0 && samples == nullptr))
        return PCM_ERR_NO_FIFO;
    const uint32_t cap = f->cfg.capacity_samples;
    const uint32_t free_slots = cap - f->level;
    const uint32_t accepted =
        count < free_slots ? count : free_slots;
    for (uint32_t k = 0; k < accepted; ++k) {
        f->ring[f->head] = samples[k];
        f->head = (f->head + 1) % cap;
    }
    f->level += accepted;
    if (out_accepted) *out_accepted = accepted;
    return PCM_OK;
//@LABS-STUB
    // TODO(2): accept min(count, free space) samples at the head, wrap the
    // head index modulo capacity, bump level, report the accepted count.
    // Partial acceptance is the correct behavior under back-pressure.
    if (f == nullptr || (count > 0 && samples == nullptr))
        return PCM_ERR_NO_FIFO;
    if (out_accepted) *out_accepted = 0;   // wrong on purpose: drops all
    return PCM_OK;
//@LABS-END
}

int pcm_pop(PcmFifo* f, PcmSample* out, uint32_t want, uint32_t* out_got) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    if (f == nullptr || (want > 0 && out == nullptr)) return PCM_ERR_NO_FIFO;
    const uint32_t cap = f->cfg.capacity_samples;
    const uint32_t drained = want < f->level ? want : f->level;
    for (uint32_t k = 0; k < drained; ++k) {
        out[k] = f->ring[f->tail];
        f->tail = (f->tail + 1) % cap;
    }
    f->level -= drained;
    if (out_got) *out_got = drained;
    return PCM_OK;
//@LABS-STUB
    // TODO(3): drain up to `want` samples from the tail in FIFO order,
    // wrapping modulo capacity, lowering level, reporting how many came
    // out (zero when empty).
    if (f == nullptr || (want > 0 && out == nullptr)) return PCM_ERR_NO_FIFO;
    if (out_got) *out_got = want;   // wrong on purpose: invents samples
    return PCM_OK;
//@LABS-END
}

int pcm_level(const PcmFifo* f, uint32_t* out_level) {
    if (f == nullptr || out_level == nullptr) return PCM_ERR_NO_FIFO;
    *out_level = f->level;
    return PCM_OK;
}

}  // namespace nes24abi
