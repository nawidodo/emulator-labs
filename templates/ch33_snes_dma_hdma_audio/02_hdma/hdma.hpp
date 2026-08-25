#pragma once
// SNES HDMA model (chapter 33, exercise 02): direct and indirect tables,
// one register group per scanline, effects applied AT LINE START.
//
// Simplifications vs. real hardware (all documented in LECTURE.md):
//   * One flat RAM image for tables AND indirect data (no banking).
//   * Each channel writes ONE base register plus consecutive offsets
//     (1-4 registers), instead of full DMA-mode patterns.
//   * "Repeat" entries hold the last value; we model that by emitting NO
//     register writes on repeated lines (the register simply keeps its
//     value, exactly like hardware not re-triggering the transfer).
#include <cstdint>
#include <span>
#include <vector>

namespace snesdma {

inline constexpr int kVisibleLines = 224;
inline constexpr int kHdmaChannels = 8;
inline constexpr int kMaxRegsPerLine = 4;

struct ChannelConfig {
    bool enabled = false;
    bool indirect = false;   // $43x0 bit 6
    uint16_t base_reg = 0;   // $21xx B-bus register written each active line
    int regs_per_line = 1;   // writes base_reg .. base_reg+regs_per_line-1
    uint8_t bank = 0;        // $43x7 indirect bank (flattened into the image)
    uint16_t table_addr = 0; // start of this channel's HDMA table in RAM
};

// One register write produced by HDMA on some scanline. The write for line
// N is IN EFFECT during line N: consumers apply it before rendering line N.
struct RegWrite {
    int channel = 0;
    uint16_t reg = 0;
    uint8_t value = 0;
};

// Parsed HDMA table header byte:
//   $00          -> channel terminates for the rest of the frame
//   bit 7 clear  -> fresh data is fetched EVERY line for `count` lines
//   bit 7 set    -> data fetched ONCE; `count` lines reuse it ("repeat")
struct EntryHeader {
    int lines = 0;
    bool repeat = false;
    bool terminate = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline EntryHeader parse_header(uint8_t h) {
    EntryHeader e;
    if (h == 0) {
        e.terminate = true;
        return e;
    }
    e.lines = h & 0x7F;
    e.repeat = (h & 0x80) != 0;
    if (e.lines == 0) {
        // Count 0 with bit 7 set has no defined meaning on hardware; treat
        // it like termination so a malformed table cannot stall the engine.
        e.terminate = true;
    }
    return e;
}
//@LABS-STUB
// TODO(1): decode an HDMA header byte. Bit 7 = repeat flag, bits 0-6 =
// line count, $00 = terminate. A count of 0 must also terminate.
inline EntryHeader parse_header(uint8_t /*h*/) {
    EntryHeader e;
    e.lines = 1;  // wrong on purpose: never terminates, never repeats
    return e;
}
//@LABS-END

class Hdma {
public:
    void set_ram(std::span<const uint8_t> ram) { ram_ = ram; }

    void configure(int channel, const ChannelConfig& cfg) {
        cfg_[channel] = cfg;
    }

    // Frame start (hardware: V=0). Every enabled channel rewinds to the
    // start of its table; nothing has been applied yet.
    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    void init() {
        for (int ch = 0; ch < kHdmaChannels; ++ch) {
            State& st = state_[ch];
            st.cursor = size_t(cfg_[ch].table_addr);
            st.lines_left = 0;
            st.entry_valid = false;
            st.repeat_entry = false;
            st.terminated = !cfg_[ch].enabled;
        }
    }
    //@LABS-STUB
    // TODO(4): rewind every enabled channel's cursor to its table_addr and
    // clear per-entry state. Disabled channels are terminated from the start.
    void init() {}
    //@LABS-END

    // Processes scanline `line` (0-based). Returns, in channel order 0..7,
    // every register write whose effect is live DURING this line.
    //
    // Timing contract (the property the debug exercise breaks):
    //   * A write returned by run_line(n) applies at the START of line n.
    //   * An entry loaded for line n affects line n itself, never n+1.
    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    std::vector<RegWrite> run_line(int line) {
        std::vector<RegWrite> out;
        for (int ch = 0; ch < kHdmaChannels; ++ch) {
            if (!cfg_[ch].enabled || state_[ch].terminated) continue;
            State& st = state_[ch];
            // Reload the table cursor whenever the previous entry ran out;
            // entry_valid alone cannot tell "entry active" from "exhausted".
            if (st.lines_left == 0) load_entry(ch);
            if (st.terminated || st.lines_left == 0) continue;
            // Fresh data is fetched on the first line of every entry; a
            // repeat entry then holds that value silently for the rest of
            // its lines (no further writes -> register keeps the value).
            if (!st.entry_started || !st.repeat_entry) {
                auto writes = fetch_data(ch);
                out.insert(out.end(), writes.begin(), writes.end());
            }
            st.entry_started = true;
            --st.lines_left;
        }
        (void)line;  // engines advance per call; kept for API clarity
        return out;
    }
    //@LABS-STUB
    // TODO(3): process one scanline across all channels. For each enabled,
    // non-terminated channel: load the next entry if none is active, then
    // fetch + emit data unless a repeat entry already delivered its value.
    std::vector<RegWrite> run_line(int /*line*/) {
        return {};  // wrong on purpose: HDMA never fires
    }
    //@LABS-END

private:
    struct State {
        size_t cursor = 0;          // next table byte to consume
        int lines_left = 0;         // lines remaining in the active entry
        bool entry_valid = false;   // an entry header has been consumed
        bool entry_started = false; // first line of the entry processed
        bool repeat_entry = false;
        bool terminated = false;
    };

    // Consumes the next table header for `ch` (direct or indirect tables
    // share the header format). Marks the channel terminated on $00.
    void load_entry(int ch) {
        State& st = state_[ch];
        st.entry_started = false;
        if (st.cursor >= ram_.size()) {
            st.terminated = true;
            return;
        }
        const EntryHeader e = parse_header(ram_[st.cursor++]);
        if (e.terminate) {
            st.terminated = true;
            st.entry_valid = false;
            return;
        }
        st.lines_left = e.lines;
        st.repeat_entry = e.repeat;
        st.entry_valid = true;
    }

    // Reads one line's worth of data for channel `ch` and returns its
    // register writes. Direct mode: regs_per_line bytes straight from the
    // table. Indirect mode: a 2-byte little-endian pointer first, then the
    // data lives at bank:ptr inside the flat image.
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    std::vector<RegWrite> fetch_data(int ch) {
        const ChannelConfig& cfg = cfg_[ch];
        std::vector<RegWrite> out;
        size_t data_cursor = state_[ch].cursor;
        if (cfg.indirect) {
            if (data_cursor + 2 > ram_.size()) {
                state_[ch].terminated = true;
                return out;
            }
            const uint16_t ptr = uint16_t(ram_[data_cursor]) |
                                 uint16_t(ram_[data_cursor + 1]) << 8;
            data_cursor = (size_t(cfg.bank) << 16 | ptr) % ram_.size();
            state_[ch].cursor += 2;
        }
        for (int r = 0; r < cfg.regs_per_line && r < kMaxRegsPerLine; ++r) {
            if (data_cursor + size_t(r) >= ram_.size()) {
                state_[ch].terminated = true;
                break;
            }
            out.push_back({ch, uint16_t(cfg.base_reg + r),
                           ram_[data_cursor + size_t(r)]});
        }
        if (!cfg.indirect) state_[ch].cursor += size_t(cfg.regs_per_line);
        return out;
    }
    //@LABS-STUB
    // TODO(2): fetch one line's data. Direct: regs_per_line bytes from the
    // cursor, advancing it. Indirect: read a 2-byte LE pointer first, index
    // (bank << 16 | ptr) into the RAM image, and only then take the bytes.
    // Emit {channel, base_reg + r, value} for each byte r.
    std::vector<RegWrite> fetch_data(int /*ch*/) {
        return {};  // wrong on purpose: no data ever fetched
    }
    //@LABS-END

    ChannelConfig cfg_[kHdmaChannels]{};
    State state_[kHdmaChannels]{};
    std::span<const uint8_t> ram_{};
};

}  // namespace snesdma
