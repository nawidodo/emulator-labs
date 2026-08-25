#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace chip8 {

inline constexpr int kWidth = 64;
inline constexpr int kHeight = 32;
inline constexpr int kMemorySize = 4096;
inline constexpr uint16_t kProgStart = 0x200;
inline constexpr uint16_t kFontAddr = 0x050;
inline constexpr int kStackSize = 16;

// Fixed teaching rates (see docs: every golden hash depends on them):
//   600 instructions per second, 60 Hz timers, 60 fps display.
//   -> exactly 10 instructions per timer tick and per emulated frame.
inline constexpr uint32_t kCyclesPerSecond = 600;
inline constexpr uint32_t kFramesPerSecond = 60;
inline constexpr uint32_t kCyclesPerTimerTick = kCyclesPerSecond / kFramesPerSecond;  // 10
inline constexpr uint32_t kCyclesPerFrame = kCyclesPerSecond / kFramesPerSecond;      // 10

// Quirk switches where two documented real-hardware behaviours exist.
struct Chip8Quirks {
    // false (default): DXYN clips at screen edges.
    // true           : DXYN wraps around to the opposite edge.
    bool wrapping = false;
};

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
struct Display {
    bool pixels[kWidth * kHeight] = {};

    void clear() {
        for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = false;
    }
    // Out-of-bounds reads read as "off"; out-of-bounds writes are dropped.
    bool get(int x, int y) const {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
        return pixels[y * kWidth + x];
    }
    void set(int x, int y, bool v) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        pixels[y * kWidth + x] = v;
    }
};

// Clip/wrap policy for one pixel coordinate; false means "skip pixel".
inline bool locate_pixel(int x, int y, const Chip8Quirks& quirks,
                         int* out_x, int* out_y) {
    if (quirks.wrapping) {
        *out_x = ((x % kWidth) + kWidth) % kWidth;
        *out_y = ((y % kHeight) + kHeight) % kHeight;
        return true;
    }
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
    *out_x = x;
    *out_y = y;
    return true;
}

// XOR-sprite draw; returns true iff any lit pixel was erased (VF value).
inline bool draw_sprite(Display& d, const uint8_t* rows, int row_count,
                        int origin_x, int origin_y,
                        const Chip8Quirks& quirks) {
    bool collision = false;
    for (int row = 0; row < row_count; ++row) {
        const uint8_t bits = rows[row];
        for (int bit = 0; bit < 8; ++bit) {
            if (!((bits >> (7 - bit)) & 1)) continue;
            int px = origin_x + bit;
            int py = origin_y + row;
            if (!locate_pixel(px, py, quirks, &px, &py)) continue;
            const bool before = d.get(px, py);
            d.set(px, py, !before);
            collision = collision || before;
        }
    }
    return collision;
}

//@LABS-BEGIN 8
//@LABS-SOLUTION
// DXY0 core: shifts the whole framebuffer UP by `rows` (already masked to
// 0..31 by the caller). Vacated bottom rows are cleared; content pushed past
// the top edge disappears — clip semantics, wrapping never applies.
// Returns true iff any LIT pixel was lost off the top (becomes VF).
inline bool scroll_display_up(Display& d, int rows) {
    if (rows <= 0) return false;
    bool lost = false;
    for (int y = 0; y < rows && !lost; ++y)
        for (int x = 0; x < kWidth; ++x)
            if (d.get(x, y)) { lost = true; break; }
    for (int y = 0; y < kHeight; ++y) {
        const int src = y + rows;
        for (int x = 0; x < kWidth; ++x)
            d.set(x, y, src < kHeight ? d.get(x, src) : false);
    }
    return lost;
}
//@LABS-STUB
// TODO(8): unseen-spec support — implement DXY0 "scroll display up" per
// CODING_TEST.md. Shift every row up by `rows`, clear the vacated bottom,
// and report whether any lit pixel scrolled off the top. A stub that does
// nothing keeps the suite compiling but fails the behavioural cases.
inline bool scroll_display_up(Display& /*d*/, int /*rows*/) {
    return false;  // wrong on purpose
}
//@LABS-END

// ---------------------------------------------------------------------------
// Keypad
// ---------------------------------------------------------------------------
struct Keypad {
    bool down[16] = {};

    void press(int key)   { down[key & 0xF] = true; }
    void release(int key) { down[key & 0xF] = false; }
    bool is_down(int key) const { return down[key & 0xF]; }

    // FX0A hands over the lowest-numbered held key; deterministic choice
    // where real hardware was ambiguous.
    int first_down() const {
        for (int k = 0; k < 16; ++k)
            if (down[k]) return k;
        return -1;
    }
};

// Scripted input feed: one line per frame of held hex digits ('.' or blank
// = none). Applying a frame REPLACES keypad state so replays are exact.
struct InputFeed {
    std::vector<std::string> frames;

    static InputFeed parse(std::string_view text) {
        InputFeed feed;
        std::size_t pos = 0;
        while (pos < text.size()) {  // trailing \n opens no phantom frame
            std::size_t eol = text.find('\n', pos);
            if (eol == std::string_view::npos) eol = text.size();
            std::string line;
            for (std::size_t i = pos; i < eol; ++i) {
                const char c = text[i];
                if (c == '\r' || c == ' ' || c == '\t') continue;
                line.push_back(c);
            }
            if (line == ".") line.clear();
            feed.frames.push_back(line);
            if (eol == text.size()) break;
            pos = eol + 1;
        }
        return feed;
    }

    static InputFeed from_lines(const std::vector<std::string>& lines) {
        InputFeed feed;
        feed.frames = lines;
        return feed;
    }

    void apply(Keypad& keypad, std::size_t index) const {
        for (int k = 0; k < 16; ++k) keypad.down[k] = false;
        if (index >= frames.size()) return;
        for (const char c : frames[index])
            if (c >= '0' && c <= '9') keypad.down[c - '0'] = true;
            else if (c >= 'a' && c <= 'f') keypad.down[c - 'a' + 10] = true;
            else if (c >= 'A' && c <= 'F') keypad.down[c - 'A' + 10] = true;
    }
};

// ---------------------------------------------------------------------------
// Timers — 60 Hz countdowns decoupled from the CPU rate by an accumulator.
// ---------------------------------------------------------------------------
using BeepHook = std::function<void(bool started)>;

struct Timers {
    uint8_t delay = 0;
    uint8_t sound = 0;
    BeepHook on_beep;  // fired on beep start (true) and beep end (false)

    void set_delay(uint8_t v) { delay = v; }
    void set_sound(uint8_t v) {
        const bool was_silent = sound == 0;
        sound = v;
        if (was_silent && sound != 0 && on_beep) on_beep(true);
    }
    bool beeping() const { return sound != 0; }

    void tick_cycles(uint64_t cycles) {
        accumulator_ += cycles * uint64_t{kFramesPerSecond};
        while (accumulator_ >= kCyclesPerSecond) {
            accumulator_ -= kCyclesPerSecond;
            tick_once();
        }
    }

private:
    uint64_t accumulator_ = 0;

    void tick_once() {
        if (delay > 0) --delay;
        if (sound > 0) {
            --sound;
            if (sound == 0 && on_beep) on_beep(false);
        }
    }

public:
    uint64_t pending_accumulator() const { return accumulator_; }
};

// ---------------------------------------------------------------------------
// Standard hex font: 16 glyphs x 5 bytes at 0x050 (FX29 uses this).
// ---------------------------------------------------------------------------
inline constexpr uint8_t kFontData[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};

// ---------------------------------------------------------------------------
// Machine
// ---------------------------------------------------------------------------
struct Machine {
    Chip8Quirks quirks;
    Display display;
    Keypad keypad;
    Timers timers;

    uint8_t mem[kMemorySize] = {};
    uint8_t v[16] = {};
    uint16_t i = 0;
    uint16_t pc = kProgStart;
    uint16_t stack[kStackSize] = {};
    int sp = 0;

    // Deterministic PRNG state for CXNN (opcode RND). A fixed-seed LCG keeps
    // CXNN programs replayable; Math.random-style entropy would poison every
    // golden hash in the course.
    uint32_t rng_state = 0x00000001;

    // Optional per-instruction observer (headless runner tracing). Empty by
    // default; invoked before the instruction executes, with pre-execution
    // pc still current.
    std::function<void(const Machine&, uint16_t op)> on_step;

    // Total instructions executed since load(); used by tracing runners.
    uint64_t steps_done = 0;

    void load(std::span<const uint8_t> rom) {
        clear_all();
        for (std::size_t k = 0; k < rom.size() && kProgStart + k < kMemorySize; ++k)
            mem[kProgStart + k] = rom[k];
    }

    void clear_all() {
        display.clear();
        for (int k = 0; k < 16; ++k) v[k] = 0;
        i = 0;
        pc = kProgStart;
        for (int k = 0; k < kStackSize; ++k) stack[k] = 0;
        sp = 0;
        timers.set_delay(0);
        timers.set_sound(0);
        rng_state = 0x00000001;
    }

    uint8_t rng_next_u8() {
        // Numerical Recipes LCG; deterministic on every platform.
        rng_state = rng_state * 1664525u + 1013904223u;
        return uint8_t((rng_state >> 24) & 0xFF);
    }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    inline void op_cls(Machine& m) { m.display.clear(); }
//@LABS-STUB
    // TODO(1): 00E0 CLS — clear the display.
    inline void op_cls(Machine& /*m*/) {}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // DXYN: draw sprite at (VX, VY), height N from memory at I.
    // VF = 1 iff any lit pixel was erased (collision).
    inline void op_draw(Machine& m, uint8_t x, uint8_t y, uint8_t n) {
        m.v[0xF] = draw_sprite(m.display, &m.mem[m.i], n,
                               m.v[x], m.v[y], m.quirks) ? 1 : 0;
    }
//@LABS-STUB
    // TODO(2): DXYN — draw and set VF to the collision flag.
    inline void op_draw(Machine& /*m*/, uint8_t /*x*/, uint8_t /*y*/,
                        uint8_t /*n*/) {}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // EX9E / EXA1: skip next instruction if key VX is down / not down.
    inline void op_key_skip(Machine& m, uint8_t x, bool wanted) {
        const bool down = m.keypad.is_down(m.v[x]);
        if (down == wanted) m.pc = uint16_t(m.pc + 2);
    }
//@LABS-STUB
    // TODO(3): skip the next instruction when the key state matches `wanted`.
    inline void op_key_skip(Machine& /*m*/, uint8_t /*x*/, bool /*wanted*/) {}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // FX07/FX15/FX18: move values between VX and the 60 Hz timers.
    inline void op_timers(Machine& m, uint8_t x, uint8_t sub) {
        switch (sub) {
            case 0x07: m.v[x] = m.timers.delay; break;   // LD Vx, DT
            case 0x15: m.timers.set_delay(m.v[x]); break;  // LD DT, Vx
            case 0x18: m.timers.set_sound(m.v[x]); break;  // LD ST, Vx
            default: break;
        }
    }
//@LABS-STUB
    // TODO(4): FX07 Vx=delay, FX15 delay=Vx, FX18 sound=Vx.
    inline void op_timers(Machine& /*m*/, uint8_t /*x*/, uint8_t /*sub*/) {}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
    // FX0A: wait for a key. If none is held, re-execute this instruction
    // (pc does not advance) until one goes down.
    inline void op_wait_key(Machine& m, uint8_t x) {
        const int key = m.keypad.first_down();
        if (key >= 0) {
            m.v[x] = uint8_t(key);
        } else {
            m.pc = uint16_t(m.pc - 2);  // block: retry same instruction
        }
    }
//@LABS-STUB
    // TODO(5): FX0A — store a held key into VX or block until one is held.
    inline void op_wait_key(Machine& /*m*/, uint8_t /*x*/) {}
//@LABS-END

    // DXY0 "scroll display" — course-original pseudo-op (see CODING_TEST.md
    // for the specification you must implement). Dispatch is provided; the
    // core lives in scroll_display_up() below.
    inline void op_scroll(Machine& m, uint8_t y) {
        m.v[0xF] = scroll_display_up(m.display, m.v[y] & 0x1F) ? 1 : 0;
    }

    // Executes ONE instruction; returns cycles consumed (always 1 here).
    int step() {
        const uint16_t op = uint16_t(mem[pc]) << 8 | mem[pc + 1];
        if (on_step) on_step(*this, op);
        ++steps_done;
        pc = uint16_t(pc + 2);

        const uint8_t x = (op >> 8) & 0xF;
        const uint8_t y = (op >> 4) & 0xF;
        const uint8_t n = op & 0xF;
        const uint8_t nn = op & 0xFF;
        const uint16_t nnn = op & 0xFFF;

        switch (op >> 12) {
            case 0x0:
                if (op == 0x00E0) op_cls(*this);          // CLS
                else if (op == 0x00EE) {                   // RET
                    if (sp > 0) pc = stack[--sp];
                }
                break;
            case 0x1: pc = nnn; break;                     // JP nnn
            case 0x2:                                      // CALL nnn
                if (sp < kStackSize) stack[sp++] = pc;
                pc = nnn;
                break;
            case 0x3: if (v[x] == nn) pc += 2; break;      // SE Vx,nn
            case 0x4: if (v[x] != nn) pc += 2; break;      // SNE Vx,nn
            case 0x5: if (v[x] == v[y]) pc += 2; break;    // SE Vx,Vy
            case 0x6: v[x] = nn; break;                    // LD Vx,nn
            case 0x7: v[x] = uint8_t(v[x] + nn); break;    // ADD Vx,nn
            case 0x8:                                      // ALU ops
                switch (n) {
                    case 0x0: v[x] = v[y]; break;
                    case 0x1: v[x] |= v[y]; break;
                    case 0x2: v[x] &= v[y]; break;
                    case 0x3: v[x] ^= v[y]; break;
                    case 0x4: {  // ADD with carry
                        const int sum = v[x] + v[y];
                        v[0xF] = sum > 0xFF ? 1 : 0;
                        v[x] = uint8_t(sum);
                        break;
                    }
                    case 0x5: {  // SUB: VF = NOT borrow
                        const int diff = v[x] - v[y];
                        v[0xF] = diff < 0 ? 0 : 1;
                        v[x] = uint8_t(diff);
                        break;
                    }
                    case 0x6: {  // SHR (COSMAC: shift VX, VF = old bit 0)
                        const uint8_t lsb = v[x] & 1;
                        v[x] >>= 1;
                        v[0xF] = lsb;
                        break;
                    }
                    case 0x7: {  // SUBN: VX = VY - VX
                        const int diff = v[y] - v[x];
                        v[0xF] = diff < 0 ? 0 : 1;
                        v[x] = uint8_t(diff);
                        break;
                    }
                    case 0xE: {  // SHL
                        const uint8_t msb = (v[x] >> 7) & 1;
                        v[x] <<= 1;
                        v[0xF] = msb;
                        break;
                    }
                    default: break;
                }
                break;
            case 0x9: if (v[x] != v[y]) pc += 2; break;    // SNE Vx,Vy
            case 0xA: i = nnn; break;                      // LD I,nnn
            case 0xB: pc = uint16_t(nnn + v[0]); break;    // JP V0+nnn
            case 0xC: v[x] = rng_next_u8() & nn; break;    // RND (seeded LCG)
            case 0xD:
                if (n == 0) op_scroll(*this, y);  // DXY0: scroll-display
                else op_draw(*this, x, y, n);
                break;
            case 0xE:
                if (nn == 0x9E) op_key_skip(*this, x, true);
                else if (nn == 0xA1) op_key_skip(*this, x, false);
                break;
            case 0xF:
                switch (nn) {
                    case 0x07: case 0x15: case 0x18:
                        op_timers(*this, x, nn);
                        break;
                    case 0x0A: op_wait_key(*this, x); break;
                    case 0x1E: i = uint16_t(i + v[x]); break;   // ADD I,Vx
                    case 0x29:                                   // LD F,Vx
                        i = uint16_t(kFontAddr + 5 * (v[x] & 0xF));
                        break;
                    case 0x33: {                                 // BCD
                        mem[i] = uint8_t(v[x] / 100);
                        mem[i + 1] = uint8_t((v[x] / 10) % 10);
                        mem[i + 2] = uint8_t(v[x] % 10);
                        break;
                    }
                    case 0x55:                                   // store V0..Vx
                        for (int k = 0; k <= x; ++k) mem[i + k] = v[k];
                        i = uint16_t(i + x + 1);                 // COSMAC: I++
                        break;
                    case 0x65:                                   // load V0..Vx
                        for (int k = 0; k <= x; ++k) v[k] = mem[i + k];
                        i = uint16_t(i + x + 1);
                        break;
                    default: break;
                }
                break;
            default: break;
        }
        return 1;
    }

    // Deterministic driver: execute EXACTLY n_cycles instructions, ticking
    // the 60 Hz timers once per kCyclesPerTimerTick elapsed cycles.
    // Returns the number of frame boundaries crossed (cycles / frame size).
    uint64_t run(uint64_t n_cycles) {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        uint64_t ticks_due = 0;
        for (uint64_t c = 0; c < n_cycles; ++c) {
            step();
            ++ticks_due;
            if (ticks_due >= kCyclesPerTimerTick) {
                timers.tick_cycles(ticks_due);
                ticks_due = 0;
            }
        }
        timers.tick_cycles(ticks_due);  // bank the partial remainder
        return n_cycles / kCyclesPerFrame;
//@LABS-STUB
        // TODO(6): run exactly n_cycles steps, feeding the timers every
        // kCyclesPerTimerTick cycles, and return frames advanced
        // (n_cycles / kCyclesPerFrame).
        (void)n_cycles;
        return 0;
//@LABS-END
    }

    // The coding-test contract: exactly N CPU cycles AND M timer ticks,
    // fully decoupled. Cycles step the CPU without producing timer ticks;
    // then exactly timer_ticks decrements are applied.
    void run_for(uint64_t n_cycles, uint32_t timer_ticks) {
//@LABS-BEGIN 7
//@LABS-SOLUTION
        for (uint64_t c = 0; c < n_cycles; ++c) step();
        for (uint32_t t = 0; t < timer_ticks; ++t) tick_timer_once();
//@LABS-STUB
        // TODO(7): step exactly n_cycles instructions, then apply exactly
        // timer_ticks 60 Hz decrements — independent of each other.
        (void)n_cycles; (void)timer_ticks;
//@LABS-END
    }

private:
    // Single 60 Hz decrement shared by run_for; mirrors Timers::tick_once.
    void tick_timer_once() {
        // Timers owns its private accumulator, but a raw decrement here must
        // still fire beep hooks — route through a full-tick call.
        timers.tick_cycles(kCyclesPerTimerTick);
    }
};

}  // namespace chip8
