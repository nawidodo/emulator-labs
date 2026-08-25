#pragma once
// ch06 CHIP-8 core for exercise 05 (complete; quirk presets in quirks.hpp).
// Determinism contract (needed for golden traces):
//   - CXKK uses a fixed-seed LCG instead of host randomness.
//   - Delay/sound timers tick once every kTimerDivider executed instructions,
//     approximating 60 Hz at ~660 instructions/second.
//   - No wall clock, no threads.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>

#include "quirks.hpp"

namespace ch06 {

inline constexpr int kScreenW = 64;
inline constexpr int kScreenH = 32;
inline constexpr uint16_t kProgBase = 0x200;
inline constexpr uint16_t kFontBase = 0x050;
inline constexpr size_t kMemSize = 4096;
// One "frame" of headless execution = this many instructions; timers tick
// once per frame so --frames N and --cycles N*kInstrPerFrame agree.
inline constexpr int kInstrPerFrame = 11;

struct StepResult {
    uint16_t pc;   // PC *before* executing the instruction
    uint16_t op;   // fetched big-endian opcode
};

class Chip8 {
public:
    void reset(const Chip8Quirks& q = kModernQuirks) {
        quirks = &q;
        *this = Chip8{};
        quirks = &q;
    }

    // ROM image goes at 0x200; the 80-byte hex font sits at 0x050.
    void load(std::span<const uint8_t> rom) {
        if (rom.size() > kMemSize - kProgBase) rom = rom.first(kMemSize - kProgBase);
        std::memcpy(mem + kProgBase, rom.data(), rom.size());
        prog_end = uint16_t(kProgBase + rom.size());
    }

    // True once PC leaves [0x200, prog_end): callers stop stepping here.
    bool halted() const { return pc < kProgBase || pc >= prog_end; }

    StepResult step() {
        const uint16_t pc0 = pc;
        const uint16_t op = uint16_t(mem[pc0] << 8 | mem[(pc0 + 1) & 0xFFF]);
        last_op = op;
        pc = uint16_t((pc0 + 2) & 0xFFF);

        const uint8_t x = uint8_t((op >> 8) & 0xF);
        const uint8_t y = uint8_t((op >> 4) & 0xF);
        const uint8_t kk = uint8_t(op & 0xFF);
        const uint8_t n = uint8_t(op & 0xF);
        const uint16_t nnn = uint16_t(op & 0x0FFF);

        switch (op >> 12) {
            case 0x0:
                if (op == 0x00E0) {
                    std::memset(fb, 0, sizeof(fb));
                } else if (op == 0x00EE) {
                    pc = stack_[sp > 0 ? --sp : 0];
                }
                break;  // 0NNN: SYS, treated as NOP
            case 0x1: pc = nnn; break;
            case 0x2:
                if (sp < 16) stack_[sp++] = pc;
                pc = nnn;
                break;
            case 0x3: if (v[x] == kk) pc = uint16_t(pc + 2); break;
            case 0x4: if (v[x] != kk) pc = uint16_t(pc + 2); break;
            case 0x5: if (v[x] == v[y]) pc = uint16_t(pc + 2); break;
            case 0x6: v[x] = kk; break;
            case 0x7: v[x] = uint8_t(v[x] + kk); break;
            case 0x8:
                switch (n) {
                    case 0x0: v[x] = v[y]; break;
                    case 0x1: v[x] |= v[y]; break;
                    case 0x2: v[x] &= v[y]; break;
                    case 0x3: v[x] ^= v[y]; break;
                    case 0x4: {
                        const int sum = v[x] + v[y];
                        v[0xF] = sum > 0xFF ? 1 : 0;
                        v[x] = uint8_t(sum);
                        break;
                    }
                    case 0x5: {
                        const uint8_t borrow = v[x] >= v[y] ? 1 : 0;
                        v[0xF] = borrow;
                        v[x] = uint8_t(v[x] - v[y]);
                        break;
                    }
                    case 0x6: {  // shift right; VF takes the dropped bit
                        const uint8_t src = quirks->shift_uses_vy ? v[y] : v[x];
                        v[0xF] = src & 0x01;
                        v[x] = uint8_t(src >> 1);
                        break;
                    }
                    case 0x7: {
                        const uint8_t borrow = v[y] >= v[x] ? 1 : 0;
                        v[0xF] = borrow;
                        v[x] = uint8_t(v[y] - v[x]);
                        break;
                    }
                    case 0xE: {  // shift left; VF takes the dropped bit
                        const uint8_t src = quirks->shift_uses_vy ? v[y] : v[x];
                        v[0xF] = (src >> 7) & 0x01;
                        v[x] = uint8_t(src << 1);
                        break;
                    }
                    default: break;
                }
                break;
            case 0x9: if (v[x] != v[y]) pc = uint16_t(pc + 2); break;
            case 0xA: i = nnn; break;
            case 0xB:
                // BNNN: modern jumps to NNN+V0; the CHIP-48 fork indexed
                // with whatever register X the opcode named.
                pc = uint16_t(nnn + (quirks->jump_bnnn_x ? v[x] : v[0]));
                break;
            case 0xC:  // CXKK: deterministic LCG stands in for hardware noise
                rng_ = rng_ * 6364136223846793005ULL + 1442695040888963407ULL;
                v[x] = uint8_t(kk ^ uint8_t(rng_ >> 56));
                break;
            case 0xD: draw(x, y, n); break;
            case 0xE:
                if (kk == 0x9E && keys[v[x] & 0xF]) pc = uint16_t(pc + 2);
                if (kk == 0xA1 && !keys[v[x] & 0xF]) pc = uint16_t(pc + 2);
                break;
            case 0xF: ext(x, kk); break;
            default: break;
        }

        ++cycles;
        if (cycles % kInstrPerFrame == 0) {
            if (dt > 0) --dt;
            if (st > 0) --st;
        }
        return {pc0, op};
    }

    // One-line disassembly, e.g. "0200: 6200  LD   V2, 0x00".
    std::string disassemble(uint16_t addr) const;

    // State -----------------------------------------------------------------
    uint8_t v[16]{};
    uint16_t i = 0;
    uint16_t pc = kProgBase;
    uint8_t sp = 0;
    uint8_t dt = 0;
    uint8_t st = 0;
    uint64_t cycles = 0;
    uint16_t last_op = 0;
    uint16_t prog_end = kProgBase;
    uint8_t mem[kMemSize]{};
    bool fb[kScreenW * kScreenH]{};
    bool keys[16]{};
    const Chip8Quirks* quirks = &kModernQuirks;

private:
    void draw(uint8_t x, uint8_t y, uint8_t n) {
        const int ox = v[x] % kScreenW;
        const int oy = v[y] % kScreenH;
        bool collision = false;
        for (int row = 0; row < n; ++row) {
            const uint8_t bits = mem[(i + row) & 0xFFF];
            for (int colbit = 0; colbit < 8; ++colbit) {
                if (!(bits & (0x80 >> colbit))) continue;
                int px, py;
                if (quirks->wrapping) {
                    px = (ox + colbit) % kScreenW;
                    py = (oy + row) % kScreenH;
                } else {
                    px = ox + colbit;
                    py = oy + row;
                    if (px >= kScreenW || py >= kScreenH) continue;
                }
                bool& pixel = fb[py * kScreenW + px];
                if (pixel) {
                    collision = true;
                    pixel = false;
                } else {
                    pixel = true;
                }
            }
        }
        v[0xF] = collision ? 1 : 0;
    }

    void ext(uint8_t x, uint8_t kk) {
        switch (kk) {
            case 0x07: v[x] = dt; break;
            case 0x0A: {  // block until a key is down; retry same instruction
                bool any = false;
                for (int k = 0; k < 16; ++k)
                    if (keys[k]) { v[x] = uint8_t(k); any = true; break; }
                if (!any) pc = uint16_t(pc - 2);
                break;
            }
            case 0x15: dt = v[x]; break;
            case 0x18: st = v[x]; break;
            case 0x1E:
                if (quirks->vf_reset) v[0xF] = 0;
                i = uint16_t(i + v[x]);
                break;
            case 0x29: i = uint16_t(kFontBase + 5 * (v[x] & 0xF)); break;
            case 0x33:
                mem[i & 0xFFF] = uint8_t(v[x] / 100);
                mem[(i + 1) & 0xFFF] = uint8_t(v[x] / 10 % 10);
                mem[(i + 2) & 0xFFF] = uint8_t(v[x] % 10);
                break;
            case 0x55:
                if (quirks->vf_reset) v[0xF] = 0;
                for (int r = 0; r <= x; ++r) mem[(i + r) & 0xFFF] = v[r];
                if (!quirks->load_store_leaves_i) i = uint16_t(i + x + 1);
                break;
            case 0x65:
                if (quirks->vf_reset) v[0xF] = 0;
                for (int r = 0; r <= x; ++r) v[r] = mem[(i + r) & 0xFFF];
                if (!quirks->load_store_leaves_i) i = uint16_t(i + x + 1);
                break;
            default: break;
        }
    }

    uint64_t rng_ = 0x123456789ABCDEF0ULL;
    uint16_t stack_[16]{};
};
inline std::string Chip8::disassemble(uint16_t addr) const {
    auto hex = [](uint32_t value, int width) {
        char buf[8];
        snprintf(buf, sizeof buf, "%0*X", width, value);
        return std::string(buf);
    };
    const uint16_t op = uint16_t(mem[addr] << 8 | mem[(addr + 1) & 0xFFF]);
    const uint8_t x = uint8_t((op >> 8) & 0xF);
    const uint8_t y = uint8_t((op >> 4) & 0xF);
    const uint8_t kk = uint8_t(op & 0xFF);
    const uint8_t n = uint8_t(op & 0xF);
    const uint16_t nnn = uint16_t(op & 0x0FFF);
    std::string m, args;
    switch (op >> 12) {
        case 0x0:
            if (op == 0x00E0) { m = "CLS"; }
            else if (op == 0x00EE) { m = "RTS"; }
            else { m = "SYS"; args = "0x" + hex(nnn, 3); }
            break;
        case 0x1: m = "JP"; args = "0x" + hex(nnn, 3); break;
        case 0x2: m = "CALL"; args = "0x" + hex(nnn, 3); break;
        case 0x3: m = "SE"; args = "V" + hex(x, 1) + ", 0x" + hex(kk, 2); break;
        case 0x4: m = "SNE"; args = "V" + hex(x, 1) + ", 0x" + hex(kk, 2); break;
        case 0x5: m = "SE"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
        case 0x6: m = "LD"; args = "V" + hex(x, 1) + ", 0x" + hex(kk, 2); break;
        case 0x7: m = "ADD"; args = "V" + hex(x, 1) + ", 0x" + hex(kk, 2); break;
        case 0x8:
            switch (n) {
                case 0x0: m = "LD"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x1: m = "OR"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x2: m = "AND"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x3: m = "XOR"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x4: m = "ADD"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x5: m = "SUB"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0x6: m = "SHR"; args = "V" + hex(x, 1) + (quirks->shift_uses_vy ? ", V" + hex(y, 1) : ""); break;
                case 0x7: m = "SUBN"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
                case 0xE: m = "SHL"; args = "V" + hex(x, 1) + (quirks->shift_uses_vy ? ", V" + hex(y, 1) : ""); break;
                default: m = "DB"; args = "0x" + hex(op, 4); break;
            }
            break;
        case 0x9: m = "SNE"; args = "V" + hex(x, 1) + ", V" + hex(y, 1); break;
        case 0xA: m = "LD"; args = "I, 0x" + hex(nnn, 3); break;
        case 0xB:
            m = "JP";
            args = quirks->jump_bnnn_x
                       ? "V" + hex(uint32_t((op >> 8) & 0xF), 1) + ", 0x" + hex(nnn, 3)
                       : "V0, 0x" + hex(nnn, 3);
            break;
        case 0xC: m = "RND"; args = "V" + hex(x, 1) + ", 0x" + hex(kk, 2); break;
        case 0xD: m = "DRW"; args = "V" + hex(x, 1) + ", V" + hex(y, 1) + ", " + hex(n, 1); break;
        case 0xE:
            if (kk == 0x9E) { m = "SKP"; args = "V" + hex(x, 1); }
            else if (kk == 0xA1) { m = "SKNP"; args = "V" + hex(x, 1); }
            break;
        case 0xF:
            switch (kk) {
                case 0x07: m = "LD"; args = "V" + hex(x, 1) + ", DT"; break;
                case 0x0A: m = "LD"; args = "V" + hex(x, 1) + ", K"; break;
                case 0x15: m = "LD"; args = "DT, V" + hex(x, 1); break;
                case 0x18: m = "LD"; args = "ST, V" + hex(x, 1); break;
                case 0x1E: m = "ADD"; args = "I, V" + hex(x, 1); break;
                case 0x29: m = "LD"; args = "F, V" + hex(x, 1); break;
                case 0x33: m = "BCD"; args = "V" + hex(x, 1); break;
                case 0x55: m = "LD"; args = "[I], V" + hex(x, 1); break;
                case 0x65: m = "LD"; args = "V" + hex(x, 1) + ", [I]"; break;
                default: m = "DB"; args = "0x" + hex(op, 4); break;
            }
            break;
        default: m = "DB"; args = "0x" + hex(op, 4); break;
    }
    return hex(addr, 4) + ": " + hex(op, 4) + "  " + m +
           (args.empty() ? "" : " " + args);
}

}  // namespace ch06
