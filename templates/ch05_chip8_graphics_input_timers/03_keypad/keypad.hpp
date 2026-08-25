#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace chip8 {

inline constexpr int kKeyCount = 16;

// Keypad state model. CHIP-8 has 16 keys labelled 0-F; the host maps them
// onto whatever physical layout it likes (classic: 1234/QWER/ASDF/ZXCV).
struct Keypad {
    bool down[kKeyCount] = {};

    void press(int key)   { down[key & 0xF] = true; }
    void release(int key) { down[key & 0xF] = false; }
    bool is_down(int key) const { return down[key & 0xF]; }

    // FX0A hands the *lowest-numbered* held key to the program when several
    // are held at once. Real hardware is ambiguous here; picking a fixed
    // order keeps emulation deterministic.
    int first_down() const {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        for (int k = 0; k < kKeyCount; ++k)
            if (down[k]) return k;
        return -1;
//@LABS-STUB
        // TODO(1): return the lowest-numbered key currently held, or -1
        // when no key is held.
        return 5;  // wrong on purpose
//@LABS-END
    }
};

// Scripted input feed — the headless replacement for someone sitting at the
// keyboard. Input-file line protocol:
//
//   - one line per emulated frame, in order
//   - each line lists the hex digits of keys HELD during that frame,
//     e.g. "25F" holds keys 2, 5 and F
//   - "." means no keys held this frame
//   - blank lines also count as frames with no keys held
//
// The feed REPLACES keypad state each frame: keys not listed are released.
// That makes replays bit-exact regardless of how long earlier frames ran.
struct InputFeed {
    std::vector<std::string> frames;

//@LABS-BEGIN 2
//@LABS-SOLUTION
    static InputFeed parse(std::string_view text) {
        InputFeed feed;
        std::size_t pos = 0;
        // A trailing newline terminates the last line; it does not open a
        // phantom extra frame. Blank lines in the middle do count as frames.
        while (pos < text.size()) {
            std::size_t eol = text.find('\n', pos);
            if (eol == std::string_view::npos) eol = text.size();
            std::string line;
            for (std::size_t i = pos; i < eol; ++i) {
                const char c = text[i];
                if (c == '\r' || c == ' ' || c == '\t') continue;
                line.push_back(c);
            }
            // A '.' is just an explicit empty frame; normalize it away but
            // keep the frame so line numbering stays aligned with the file.
            if (line == ".") line.clear();
            feed.frames.push_back(line);
            if (eol == text.size()) break;
            pos = eol + 1;
        }
        return feed;
    }
//@LABS-STUB
    // TODO(2): split `text` into one frame per line per the protocol above.
    static InputFeed parse(std::string_view /*text*/) {
        return {};  // wrong on purpose
    }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Sets `keypad` to EXACTLY the keys held in frame `index`. Frames past
    // the end of the feed release everything.
    void apply(Keypad& keypad, std::size_t index) const {
        for (int k = 0; k < kKeyCount; ++k) keypad.down[k] = false;
        if (index >= frames.size()) return;
        for (const char c : frames[index])
            if (c >= '0' && c <= '9') keypad.down[c - '0'] = true;
            else if (c >= 'a' && c <= 'f') keypad.down[c - 'a' + 10] = true;
            else if (c >= 'A' && c <= 'F') keypad.down[c - 'A' + 10] = true;
    }
//@LABS-STUB
    // TODO(3): press every key named by frame `index` and release all others.
    void apply(Keypad& /*keypad*/, std::size_t /*index*/) const {}
//@LABS-END
};

// Pure model of the FX0A wait-for-key instruction over a scripted feed:
// returns the frame index at which a key first goes down while waiting and
// writes it to `key_out`; returns -1 if the feed ends before any key press.
// `start_frame` is where waiting begins.
inline int wait_for_key(const InputFeed& feed, int start_frame,
                        int* key_out) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    Keypad probe;
    for (std::size_t f = static_cast<std::size_t>(start_frame);
         f < feed.frames.size(); ++f) {
        feed.apply(probe, f);
        const int key = probe.first_down();
        if (key >= 0) {
            *key_out = key;
            return static_cast<int>(f);
        }
    }
    return -1;
//@LABS-STUB
    // TODO(4): walk the feed from start_frame; on the first frame with any
    // key held, store the key and return that frame index (-1 if none).
    (void)feed; (void)start_frame; (void)key_out;
    return -2;  // wrong on purpose
//@LABS-END
}

}  // namespace chip8
