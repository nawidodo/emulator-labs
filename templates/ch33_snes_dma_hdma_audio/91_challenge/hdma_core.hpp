#pragma once
// Provided HDMA engine for the challenge (no @LABS blocks here -- the
// student's job in 91_challenge is bundle integration, not re-deriving the
// engine; that was exercise 02). Semantics identical to 02_hdma/hdma.hpp:
// effects apply AT LINE START, repeat entries hold their value silently.
#include "bundle.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace snesdma::challenge {

inline constexpr int kVisibleLines = 224;
inline constexpr int kChannels = 8;

struct ChannelSetup {
    bool enabled = false;
    bool indirect = false;
    uint16_t base_reg = 0;
    int regs_per_line = 1;
    uint8_t bank = 0;
    size_t table_off = 0;
    size_t table_len = 0;
};

// One effect event with its scanline attached (drives --trace and the
// per-line effect buffer).
struct LineEffect {
    int line = 0;
    int channel = 0;
    uint16_t reg = 0;
    uint8_t value = 0;
};

class HdmaCore {
public:
    void set_blob(const std::vector<uint8_t>* blob) { blob_ = blob; }

    void configure(int ch, const ChannelSetup& setup) { cfg_[ch] = setup; }

    void init() {
        for (int c = 0; c < kChannels; ++c) {
            St& st = st_[c];
            st.cursor = cfg_[c].table_off;
            st.end = cfg_[c].table_off + cfg_[c].table_len;
            st.lines_left = 0;
            st.entry_started = false;
            st.repeat_entry = false;
            st.terminated = !cfg_[c].enabled;
        }
    }

    // Applies scanline `line` and appends every register write to `log`.
    void run_line(int line, std::vector<LineEffect>& log) {
        for (int c = 0; c < kChannels; ++c) {
            if (!cfg_[c].enabled || st_[c].terminated) continue;
            St& st = st_[c];
            if (st.lines_left == 0 && !load_entry(st)) continue;
            const ChannelSetup& cfg = cfg_[c];
            if (!st.entry_started || !st.repeat_entry) {
                size_t data = st.cursor;
                if (cfg.indirect) {
                    if (data + 2 > blob_->size()) {
                        st.terminated = true;
                        continue;
                    }
                    const uint16_t ptr = uint16_t((*blob_)[data]) |
                                         uint16_t((*blob_)[data + 1]) << 8;
                    data = (size_t(cfg.bank) << 16 | ptr) % blob_->size();
                    st.cursor += 2;
                }
                for (int r = 0; r < cfg.regs_per_line && r < 4; ++r) {
                    if (data + size_t(r) >= blob_->size()) {
                        st.terminated = true;
                        break;
                    }
                    log.push_back({line, c,
                                   uint16_t(cfg.base_reg + r),
                                   (*blob_)[data + size_t(r)]});
                }
                if (!cfg.indirect) st.cursor += size_t(cfg.regs_per_line);
            }
            st.entry_started = true;
            --st.lines_left;
        }
    }

private:
    struct St {
        size_t cursor = 0;
        size_t end = 0;
        int lines_left = 0;
        bool entry_started = false;
        bool repeat_entry = false;
        bool terminated = false;
    };

    bool load_entry(St& st) {
        st.entry_started = false;
        if (st.cursor >= st.end || st.cursor >= blob_->size()) {
            st.terminated = true;
            return false;
        }
        const uint8_t h = (*blob_)[st.cursor++];
        if (h == 0 || (h & 0x7F) == 0) {
            st.terminated = true;
            return false;
        }
        st.lines_left = h & 0x7F;
        st.repeat_entry = (h & 0x80) != 0;
        return true;
    }

    std::array<ChannelSetup, kChannels> cfg_{};
    std::array<St, kChannels> st_{};
    const std::vector<uint8_t>* blob_ = nullptr;
};

}  // namespace snesdma::challenge
