#pragma once
//
// ch43 / 91_challenge — push GPU command lists through linked-list DMA
// into a tiny deterministic framebuffer ("VRAM") and hash the result.
//
// Packet payload word 0 is the command word; opcode = top byte:
//   0x02 FillRect: w1 = rgb15 color (low 16 bits)
//                  w2 = x<<16 | y      (signed 16-bit coords, clamped)
//                  w3 = width<<16 | height
//   other opcodes are ignored (their words are still consumed).
//
// The hash is FNV-1a 64 over the raw VRAM bytes — identical to the digest
// used by tools/labs/hash_frame.py and hidden manifests.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../03_linked_list/linked_list.hpp"

namespace ps1chal {

inline uint64_t fnv1a64(const void* data, size_t len) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

struct Vram {
    static constexpr unsigned kW = 64;
    static constexpr unsigned kH = 32;
    std::vector<uint16_t> px;

    Vram() : px(size_t(kW) * kH, 0u) {}

    void fill_rect(uint16_t color15, int x, int y, int w, int h) {
        for (int yy = y; yy < y + h; ++yy) {
            if (yy < 0 || yy >= int(kH)) continue;
            for (int xx = x; xx < x + w; ++xx) {
                if (xx < 0 || xx >= int(kW)) continue;
                px[size_t(yy) * kW + xx] = color15;
            }
        }
    }
};

class MiniGpu : public ps1::GpuSink {
public:
    explicit MiniGpu(Vram& vram) : vram_(vram) {}
    void push_packet_word(uint32_t w) override {
        buf_.push_back(w);
        if (buf_.size() == 1) {
            const uint32_t op = w >> 24;
            expect_ = op == 0x02 ? 3u : 1u;  // cmd+color, xy, wh
        }
        if (buf_.size() < expect_) return;
        execute();
        buf_.clear();
    }

private:
    void execute() {
        const uint32_t cmd = buf_[0];
        switch (cmd >> 24) {
            case 0x02: {
                const uint16_t color = uint16_t(cmd & 0xFFFFu);
                const int x = int16_t(buf_[1] >> 16);
                const int y = int16_t(buf_[1] & 0xFFFFu);
                const int wd = int16_t(buf_[2] >> 16);
                const int ht = int16_t(buf_[2] & 0xFFFFu);
                vram_.fill_rect(color, x, y, wd, ht);
                break;
            }
            default:
                break;  // unknown opcode: consumed, ignored
        }
    }

    Vram& vram_;
    std::vector<uint32_t> buf_;
    unsigned expect_ = 1;
};

struct ChallengeResult {
    ps1::WalkResult walk;
    uint64_t vram_fnv = 0;
};

// Feed a whole chain through DMA into the mini-GPU and hash VRAM.
inline ChallengeResult run_list(ps1::Ram& ram, uint32_t madr, Vram& vram,
                                ps1::TraceLog* trace = nullptr) {
    MiniGpu gpu(vram);
    ChallengeResult out;
    out.walk = ps1::walk_gpu_list(ram, madr, gpu, trace);
    out.vram_fnv = fnv1a64(vram.px.data(), vram.px.size() * 2);
    return out;
}

}  // namespace ps1chal
