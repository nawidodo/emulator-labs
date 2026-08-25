#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "shift_register.hpp"

// Exercise 3 — the port-mapped I/O space: input latches, the scripted
// input-file protocol, sound-port event recording and the watchdog.
//
// Port map (chapter-defined, documented in SPEC.md):
//
//   IN  0 : coin / service latch
//   IN  1 : fire / move / start latch (bit0 left, bit1 right, bit2 fire,
//           bit3 1P start, bit4 2P start)
//   IN  2 : dip-switch latch
//   IN  3 : SHIFT REGISTER result (read-only view of exercise 2's part)
//   OUT 2 : bits 0-2 -> shifter amount (upper bits ignored)
//   OUT 3 : sound event -> recorder
//   OUT 4 : shift register data write
//   OUT 5 : sound event -> recorder
//   OUT 6 : watchdog kick (also recorded as a port-6 event)
//
// Sound ports are DOCUMENTED STUBS: instead of synthesizing audio they
// append (cycle, port, value) events. The event log IS the contract.
// Unassigned ports read 0x00 and swallow writes.

namespace si {

struct InputFrame {
    uint8_t port0 = 0, port1 = 0, port2 = 0;
};

struct SoundEvent {
    uint64_t cycle;
    uint8_t port;
    uint8_t value;
};

class SoundRecorder {
public:
    void record(uint64_t cycle, uint8_t port, uint8_t value) {
        events_.push_back({cycle, port, value});
    }
    const std::vector<SoundEvent>& events() const { return events_; }
    void clear() { events_.clear(); }

private:
    std::vector<SoundEvent> events_;
};

class Watchdog {
public:
    void kick(uint64_t cycle) { last_kick_ = cycle; kicked_ = true; }
    bool kicked() const { return kicked_; }
    uint64_t last_kick() const { return last_kick_; }
    bool expired(uint64_t cycle, uint64_t timeout) const {
        return kicked_ && (cycle - last_kick_ > timeout);
    }

private:
    uint64_t last_kick_ = 0;
    bool kicked_ = false;
};

// Scripted input protocol: ONE LINE PER FRAME PERIOD, three hex bytes
// "P0 P1 P2" (missing trailing bytes default to 00). Blank lines and
// '#' comments are skipped so files stay readable. Returns false on any
// malformed token — a silently misparsed input file would poison every
// downstream golden.
inline bool parse_input_script(const std::string& text,
                               std::vector<InputFrame>* out) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    out->clear();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream ls(line);
        unsigned v = 0;
        InputFrame f;
        if (!(ls >> std::hex >> v)) continue;   // blank / comment-only
        f.port0 = uint8_t(v);
        if (!(ls >> std::hex >> v)) return false;   // partial line: error
        f.port1 = uint8_t(v);
        if (!(ls >> std::hex >> v)) return false;
        f.port2 = uint8_t(v);
        out->push_back(f);
    }
    return true;
//@LABS-STUB
    // TODO(1): parse one "P0 P1 P2" hex triple per line ('#' comments and
    // blank lines skipped; missing P2/P1 is an ERROR -> return false),
    // appending one InputFrame per line to `out`. Return true when the
    // whole script parsed.
    (void)text;
    (void)out;
    return false;  // wrong on purpose: nothing parses
//@LABS-END
}

class IoPorts {
public:
    void attach(ShiftRegister* sr, SoundRecorder* snd, Watchdog* wdt) {
        shifter_ = sr;
        sound_ = snd;
        watchdog_ = wdt;
    }

    void set_inputs(const InputFrame& f) { inputs_ = f; }
    const InputFrame& inputs() const { return inputs_; }

    uint8_t in(uint8_t port) const {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        switch (port) {
            case 0: return inputs_.port0;
            case 1: return inputs_.port1;
            case 2: return inputs_.port2;
            case 3: return shifter_ ? shifter_->read() : uint8_t(0x00);
            default: return 0x00;
        }
//@LABS-STUB
        // TODO(2): decode IN ports 0/1/2 to the input latches and port 3
        // to the shift-register result; unassigned ports read 0x00.
        (void)port;
        return 0x00;  // wrong on purpose: everything floats low
//@LABS-END
    }

    void out(uint8_t port, uint8_t val, uint64_t cycle) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        switch (port) {
            case 2:
                if (shifter_) shifter_->set_amount(val);
                break;
            case 3:
            case 5:
                if (sound_) sound_->record(cycle, port, val);
                break;
            case 4:
                if (shifter_) shifter_->write_data(val);
                break;
            case 6:
                if (watchdog_) watchdog_->kick(cycle);
                if (sound_) sound_->record(cycle, port, val);
                break;
            default:
                break;   // unassigned ports: write vanishes
        }
//@LABS-STUB
        // TODO(3): route OUT writes — amount latch (port 2, low bits),
        // shifter data (4), sound events into the recorder (3 and 5),
        // watchdog kick + event (6). Unassigned ports drop the write.
        (void)port;
        (void)val;
        (void)cycle;
//@LABS-END
    }

private:
    InputFrame inputs_{};
    ShiftRegister* shifter_ = nullptr;
    SoundRecorder* sound_ = nullptr;
    Watchdog* watchdog_ = nullptr;
};

}  // namespace si
