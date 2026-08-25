// core.cpp — opaque-handle implementation behind the ABI header.
// The @LABS blocks are the student's work; everything outside them is
// scaffolding shared by skeleton and reference.
#include "abi_boundary.hpp"

#include <cstring>
#include <new>

namespace chip8abi {

// The real machine layout lives here — callers see only the opaque
// handle declared in the header.
struct Ch8Machine {
    Ch8Config cfg{};
    uint8_t mem[4096]{};
    uint8_t v[16]{};
    uint16_t i = 0;
    uint16_t pc = 0x200;
    uint8_t dt = 0;
    uint8_t st = 0;
    uint8_t fb[64 * 32]{};
    uint64_t frames = 0;
};

namespace {

constexpr int kEntry = 0x200;
constexpr int kCyclesPerOp = 10;

}  // namespace

Ch8Machine* ch8_create(const Ch8Config* cfg, int* out_err) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (cfg == nullptr || cfg->struct_size != sizeof(Ch8Config)) {
        if (out_err) *out_err = CH8_ERR_SIZE;
        return nullptr;
    }
    if (cfg->abi_version != kAbiVersion) {
        if (out_err) *out_err = CH8_ERR_VERSION;
        return nullptr;
    }
    Ch8Machine* m = new (std::nothrow) Ch8Machine();
    if (!m) {
        if (out_err) *out_err = CH8_ERR_SIZE;
        return nullptr;
    }
    m->cfg = *cfg;
    if (out_err) *out_err = CH8_OK;
    return m;
//@LABS-STUB
    // TODO(1): validate cfg (struct_size must equal sizeof(Ch8Config),
    // abi_version must equal kAbiVersion), then allocate a zeroed machine,
    // store the config, set *out_err = CH8_OK and return the handle.
    // Wrong version -> CH8_ERR_VERSION; bad/null size -> CH8_ERR_SIZE.
    (void)cfg;
    if (out_err) *out_err = CH8_ERR_VERSION;  // wrong on purpose: always
    return nullptr;                           // refuses every config
//@LABS-END
}

void ch8_destroy(Ch8Machine* m) { delete m; }

int ch8_load_rom(Ch8Machine* m, const uint8_t* bytes, uint16_t size) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    if (m == nullptr || bytes == nullptr || size == 0)
        return CH8_ERR_NO_ROM;
    if (size > kRomMaxBytes) return CH8_ERR_ROM_TOO_BIG;
    std::memcpy(m->mem + kEntry, bytes, size);
    m->pc = kEntry;   // loading a ROM rewinds the machine to entry
    return CH8_OK;
//@LABS-STUB
    // TODO(2): reject null/zero-size with CH8_ERR_NO_ROM, reject
    // size > kRomMaxBytes with CH8_ERR_ROM_TOO_BIG, copy the bytes to
    // 0x200 and reset PC to the entry point.
    if (m == nullptr) return CH8_ERR_NO_ROM;
    std::memcpy(m->mem + kEntry, bytes,
                size > kRomMaxBytes ? kRomMaxBytes : size);  // silent
                                                                 // truncate!
    return CH8_OK;  // wrong on purpose: never rejects oversize, never
                    // rewinds PC
//@LABS-END
}

namespace {

void exec_one(Ch8Machine* m) {
    const uint16_t pc = m->pc;
    const uint16_t op =
        static_cast<uint16_t>(m->mem[pc] << 8 | m->mem[(pc + 1) & 0xFFF]);
    m->pc = static_cast<uint16_t>((pc + 2) & 0xFFF);
    switch (op >> 12) {
        case 0x0:
            if (op == 0x00E0)
                std::memset(m->fb, 0, sizeof(m->fb));
            break;
        case 0x6:
            m->v[(op >> 8) & 0xF] = static_cast<uint8_t>(op & 0xFF);
            break;
        case 0x7:
            m->v[(op >> 8) & 0xF] =
                static_cast<uint8_t>(m->v[(op >> 8) & 0xF] + (op & 0xFF));
            break;
        case 0xA:
            m->i = static_cast<uint16_t>(op & 0xFFF);
            break;
        case 0xD: {
            // DXYN: XOR an n-row sprite from I into the framebuffer at
            // (VX,VY); VF = 1 when any lit pixel was erased. Clipped.
            const uint8_t vx = m->v[(op >> 8) & 0xF] % 64;
            const uint8_t vy = m->v[(op >> 8) & 0xF] % 32;
            const uint8_t rows = op & 0xF;
            m->v[0xF] = 0;
            for (uint8_t r = 0; r < rows; ++r) {
                const uint8_t bits = m->mem[(m->i + r) & 0xFFF];
                for (uint8_t b = 0; b < 8; ++b) {
                    if (!((bits >> (7 - b)) & 1)) continue;
                    const int x = vx + b;
                    const int y = vy + r;
                    if (x >= 64 || y >= 32) continue;   // clipped
                    uint8_t& px = m->fb[y * 64 + x];
                    if (px) m->v[0xF] = 1;
                    px ^= 1;
                }
            }
            break;
        }
        case 0xF:
            if ((op & 0xFF) == 0x15) m->dt = m->v[(op >> 8) & 0xF];
            break;
        default:
            break;
    }
}

}  // namespace

int ch8_run_frame(Ch8Machine* m, uint16_t keypad_mask) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    if (m == nullptr) return CH8_ERR_NO_ROM;
    (void)keypad_mask;   // keypad wiring arrives with exercise 05 of ch05
    // Hardware order: the 60 Hz tick lands at the frame boundary BEFORE
    // the CPU budget runs, so a DT value written by this frame's program
    // survives until the next boundary.
    if (m->dt > 0) --m->dt;
    if (m->st > 0) --m->st;
    uint32_t budget = m->cfg.cycles_per_frame;
    while (budget >= kCyclesPerOp) {
        exec_one(m);
        budget -= kCyclesPerOp;
    }
    ++m->frames;
    return CH8_OK;
//@LABS-STUB
    // TODO(3): spend cfg.cycles_per_frame on decoded instructions
    // (kCyclesPerOp each), then tick the 60 Hz delay/sound timers down
    // once and bump the frame counter.
    (void)keypad_mask;
    if (m == nullptr) return CH8_ERR_NO_ROM;
    ++m->frames;  // wrong on purpose: burns no cycles, ticks no timers
    return CH8_OK;
//@LABS-END
}

int ch8_read_frame(const Ch8Machine* m, uint8_t* out_2048) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    if (m == nullptr || out_2048 == nullptr) return CH8_ERR_SIZE;
    std::memcpy(out_2048, m->fb, sizeof(m->fb));
    return CH8_OK;
//@LABS-STUB
    // TODO(4): guard args and copy the 64x32 framebuffer out.
    (void)m;
    (void)out_2048;
    return CH8_ERR_SIZE;  // wrong on purpose: always refuses
//@LABS-END
}

int ch8_read_delay_timer(const Ch8Machine* m, uint8_t* out) {
    if (m == nullptr || out == nullptr) return CH8_ERR_SIZE;
    *out = m->dt;
    return CH8_OK;
}

}  // namespace chip8abi
