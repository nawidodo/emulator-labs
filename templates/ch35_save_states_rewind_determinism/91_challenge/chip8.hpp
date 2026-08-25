#pragma once
// Compact CHIP-8 core (standard opcode set, deterministic xorshift RNG
// for CXNN). Everything that influences future behavior lives here and
// therefore belongs in a save state.
#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace chip8 {

inline constexpr int kW = 64;
inline constexpr int kH = 32;
inline constexpr uint16_t kPcStart = 0x200;

struct Machine {
    std::array<uint8_t, 4096> mem{};
    std::array<uint8_t, 16> v{};
    std::array<uint16_t, 16> stack{};
    uint16_t i = 0;
    uint16_t pc = kPcStart;
    uint8_t sp = 0;
    uint8_t dt = 0;   // delay timer (decremented at frame rate)
    uint8_t st = 0;   // sound timer
    uint8_t wait_key = 0xFF;  // 0xFF = not waiting; else VX awaits key
    uint16_t keys = 0;        // bitmask of held keys 0x0..0xF
    std::array<uint8_t, kW * kH> fb{};
    uint32_t rng = 0x1B7F3C2Au;  // xorshift state — PART of machine state

    void reset() {
        mem.fill(0);
        v.fill(0);
        stack.fill(0);
        i = 0;
        pc = kPcStart;
        sp = 0;
        dt = st = 0;
        wait_key = 0xFF;
        keys = 0;
        fb.fill(0);
        rng = 0x1B7F3C2Au;
    }

    void load(std::span<const uint8_t> rom) {
        for (size_t o = 0; o < rom.size() && kPcStart + o < mem.size(); ++o)
            mem[kPcStart + o] = rom[o];
    }

    // One instruction. Returns false when the machine cannot continue
    // (unknown opcode treated as halt).
    bool step() {
        if (pc + 1 >= mem.size()) return false;
        const uint16_t op = uint16_t(mem[pc]) << 8 | mem[pc + 1];
        const uint8_t x = (op >> 8) & 0xF;
        const uint8_t y = (op >> 4) & 0xF;
        const uint8_t kk = op & 0xFF;
        const uint16_t nnn = op & 0xFFF;
        const uint8_t n = op & 0xF;
        pc += 2;
        switch (op >> 12) {
            case 0x0:
                if (op == 0x00E0) fb.fill(0);
                break;                       // 00E0 CLS; other 0xxx ignored
            case 0x1: pc = nnn; break;       // JP
            case 0x2:
                stack[sp++ % 16] = pc;
                pc = nnn;
                break;                       // CALL
            case 0x3: if (v[x] == kk) pc += 2; break;
            case 0x4: if (v[x] != kk) pc += 2; break;
            case 0x5: if (v[x] == v[y]) pc += 2; break;
            case 0x6: v[x] = kk; break;      // LD VX,kk
            case 0x7: v[x] += kk; break;     // ADD VX,kk
            case 0x8: {
                switch (n) {
                    case 0: v[x] = v[y]; break;
                    case 1: v[x] |= v[y]; break;
                    case 2: v[x] &= v[y]; break;
                    case 3: v[x] ^= v[y]; break;
                    case 4: {
                        const uint16_t s = uint16_t(v[x]) + v[y];
                        v[x] = uint8_t(s);
                        v[0xF] = s > 0xFF;
                        break;
                    }
                    case 5: {
                        const uint8_t vy = v[y];
                        v[0xF] = v[x] >= vy;
                        v[x] -= vy;
                        break;
                    }
                    case 6: v[0xF] = v[x] & 1; v[x] >>= 1; break;
                    case 7: {
                        const uint8_t vx = v[x];
                        v[0xF] = v[y] >= vx;
                        v[x] = v[y] - vx;
                        break;
                    }
                    case 0xE: v[0xF] = v[x] >> 7; v[x] <<= 1; break;
                    default: return false;
                }
                break;
            }
            case 0x9: if (v[x] != v[y]) pc += 2; break;
            case 0xA: i = nnn; break;        // LD I,nnn
            case 0xB: pc = nnn + v[0]; break;
            case 0xC: {                      // RND VX,kk — seeded, in state
                rng ^= rng << 13;
                rng ^= rng >> 17;
                rng ^= rng << 5;
                v[x] = uint8_t(rng) & kk;
                break;
            }
            case 0xD: {                      // DRW VX,VY,n (XOR sprites)
                v[0xF] = 0;
                for (uint8_t row = 0; row < n; ++row) {
                    const uint8_t sprite = mem[(i + row) & 0xFFF];
                    const int py = (v[y] + row) % kH;
                    for (uint8_t col = 0; col < 8; ++col) {
                        if (!(sprite & (0x80 >> col))) continue;
                        const int px = (v[x] + col) % kW;
                        uint8_t& pxl = fb[size_t(py) * kW + px];
                        if (pxl) v[0xF] = 1;
                        pxl ^= 1;
                    }
                }
                break;
            }
            case 0xE:
                if (kk == 0x9E || kk == 0xA1) {
                    const bool down = keys & (1u << (v[x] & 0xF));
                    if (kk == 0x9E ? down : !down) pc += 2;
                }
                break;
            case 0xF:
                switch (kk) {
                    case 0x07: v[x] = dt; break;
                    case 0x15: dt = v[x]; break;
                    case 0x18: st = v[x]; break;
                    case 0x1E: i += v[x]; break;
                    case 0x0A:
                        if (keys) {
                            for (int k = 0; k < 16; ++k) {
                                if (keys & (1u << k)) {
                                    v[x] = uint8_t(k);
                                    break;
                                }
                            }
                        } else {
                            pc -= 2;  // block until a key is held
                        }
                        break;
                    case 0x29: i = uint16_t(v[x] * 5); break;
                    case 0x33: {  // BCD
                        mem[i & 0xFFF] = v[x] / 100;
                        mem[(i + 1) & 0xFFF] = (v[x] / 10) % 10;
                        mem[(i + 2) & 0xFFF] = v[x] % 10;
                        break;
                    }
                    case 0x55:
                        for (int r = 0; r <= x; ++r) mem[(i + r) & 0xFFF] = v[r];
                        break;
                    case 0x65:
                        for (int r = 0; r <= x; ++r) v[r] = mem[(i + r) & 0xFFF];
                        break;
                    default: return false;
                }
                break;
        }
        return true;
    }

    // One frame: 10 instructions + 60 Hz timer tick.
    void frame() {
        for (int s = 0; s < 10 && step(); ++s) {}
        if (dt > 0) --dt;
        if (st > 0) --st;
    }
};

}  // namespace chip8
