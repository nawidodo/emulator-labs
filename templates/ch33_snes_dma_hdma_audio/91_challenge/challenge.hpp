#pragma once
// Student-facing glue for the challenge: turn bundle CONFIG text into
// channel setups, and turn the per-line write log into the 224-byte effect
// buffer that --hash-frame emits.
#include "bundle.hpp"
#include "hdma_core.hpp"
#include <cstddef>

#include <span>
#include <vector>

namespace snesdma::challenge {

// Parses all chN.* lines out of the config text and returns up to 8
// channel setups, indexed by channel number. Unknown keys are ignored;
// missing keys keep their defaults. `watch` receives the watch=RRRR value
// (defaults to $2100 when the key is absent).
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline std::vector<ChannelSetup> parse_channels(std::string_view config,
                                                uint16_t& watch) {
    std::vector<ChannelSetup> out;
    if (out.size() < size_t(kChannels)) out.resize(size_t(kChannels));
    watch = 0x2100;
    size_t start = 0;
    while (start <= config.size()) {
        const size_t stop = config.find('\n', start);
        const auto line = trim(config.substr(
            start, stop == std::string_view::npos ? std::string_view::npos
                                                  : stop - start));
        start = stop == std::string_view::npos ? config.size() + 1 : stop + 1;
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const auto key = line.substr(0, eq);
        const auto val = line.substr(eq + 1);
        if (key == "watch") {
            watch = uint16_t(parse_hex(val));
            continue;
        }
        if (key.size() < 5 || !key.starts_with("ch")) continue;
        int ch = 0;
        size_t i = 2;
        for (; i < key.size() && key[i] >= '0' && key[i] <= '9'; ++i) {
            ch = ch * 10 + (key[i] - '0');
        }
        if (i >= key.size() || key[i] != '.' || ch >= kChannels) continue;
        ChannelSetup& s = out[size_t(ch)];
        const auto field = key.substr(i + 1);
        if (field == "enable") {
            s.enabled = val == "1";
        } else if (field == "reg") {
            s.base_reg = uint16_t(parse_hex(val));
        } else if (field == "regs") {
            s.regs_per_line = parse_hex(val);
            if (s.regs_per_line < 1) s.regs_per_line = 1;
            if (s.regs_per_line > 4) s.regs_per_line = 4;
        } else if (field == "indirect") {
            s.indirect = val == "1";
        } else if (field == "bank") {
            s.bank = uint8_t(parse_hex(val));
        } else if (field == "table") {
            const size_t colon = val.find(':');
            if (colon != std::string_view::npos) {
                s.table_off = size_t(parse_hex(val.substr(0, colon)));
                s.table_len = size_t(parse_hex(val.substr(colon + 1)));
            }
        }
    }
    return out;
}
//@LABS-STUB
// TODO(1): split the config into lines, skip blanks/#comments, and fill a
// ChannelSetup per chN.* line (enable/reg/regs/indirect/bank/table where
// table=OFF:LEN). Also read watch=RRRR into `watch`. See bundle.hpp for
// the exact grammar.
inline std::vector<ChannelSetup> parse_channels(std::string_view /*config*/,
                                                uint16_t& /*watch*/) {
    return {};  // wrong on purpose: no channels configured
}
//@LABS-END

// Builds the 224-byte per-line effect buffer from the frame's write log.
// buf[n] = value the watched register held AFTER processing line n:
// start at 0 and let every write whose reg matches `watch_reg` overwrite
// the current value from its line onward. Log entries must be in
// ascending line order (the engine guarantees this).
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void build_effect_buffer(const std::vector<LineEffect>& log,
                                uint16_t watch_reg,
                                std::span<uint8_t> buf) {
    uint8_t current = 0;
    size_t li = 0;
    for (int n = 0; n < kVisibleLines; ++n) {
        while (li < log.size() && log[li].line == n) {
            if (log[li].reg == watch_reg) current = log[li].value;
            ++li;
        }
        buf[size_t(n)] = current;
    }
}
//@LABS-STUB
// TODO(2): walk the ordered log once, tracking the watched register's
// current value, and stamp buf[line] after applying that line's writes.
// Lines before any write stay 0.
inline void build_effect_buffer(const std::vector<LineEffect>& /*log*/,
                                uint16_t /*watch_reg*/,
                                std::span<uint8_t> /*buf*/) {}
//@LABS-END

}  // namespace snesdma::challenge
