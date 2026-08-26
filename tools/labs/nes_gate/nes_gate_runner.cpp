// nes_gate_runner.cpp — canonical whole-machine NES reference (ch52 gate).
//
// Composes the VERIFIED course components (solution tree) into a
// deterministic reference machine:
//   ch19  nes6502::Cpu           — full 6502 core (vector flow, dummy
//                                  accesses, NMI/IRQ edge semantics)
//   ch20  nesrom::NROM           — iNES header + mapper 0 cartridge
//   ch22  nes22scroll / nes22prio — loopy scroll latches + frame renderer
//   ch24  nes24sync::ApuLite     — APU channels + frame counter; the
//                                  audio contract is one s16 sample per
//                                  CPU cycle (mix()*512)
//
// Machine contract (matches the ch24 course model):
//   * every CPU cycle advances the PPU exactly 3 dots and ticks the APU
//     once; one audio sample lands per CPU cycle
//   * OAM DMA ($4014) costs 513 CPU cycles on an even cycle, 514 on odd
//   * frame = 262 scanlines x 341 dots; NMI edge at vblank (scanline 241),
//     cleared by a $2002 read; sprites are out of scope for hashing
//     (documented ch24 simplification); frame output comes from
//     nes22prio::render_frame (loopy-driven background + palette)
//
// Outputs (EMU_GATE_V1, see EMU_GATE_V1.md):
//   --gate FILE   checkpoint text (regs, CPU cycle count, FNV-1a 64 of
//                 RAM/PPU state/frame RGBA/audio segment)
//   --hash-frame  raw 256x240x4 RGBA8 frame
//   --audio-out   mono s16le PCM segment
//   --selfcheck-replay  run twice, require byte-identical checkpoints
//
// The homebrew ROM is authored in-trust (gen_homebrew.py + committed
// .nes); goldens are minted from this runner and committed with a
// provenance note (tests/public/ch52_nes_playable_gate/goldens/).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ch19_nes_cpu_accuracy/99_coding_test/cpu.hpp"
#include "ch20_nes_bus_cartridges/02_ines_nrom/nesrom.hpp"
#include "ch22_nes_ppu2_scrolling_sprites/03_sprites_priority/render22.hpp"
#include "ch24_nes_apu_dma_sync/02_frame_dma_scheduler/machine.hpp"

namespace nesgate {

constexpr int kPpuDotsPerCpu = nes24sync::kPpuDotsPerCpu;  // 3 (ch24)
constexpr int kFrameW = nes22prio::kFrameW;                // 256
constexpr int kFrameH = nes22prio::kFrameH;                // 240
constexpr size_t kFbBytes = size_t(kFrameW) * kFrameH * 4;

uint64_t fnv1a64(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

std::string fnv_hex(uint64_t h) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llX",
                  static_cast<unsigned long long>(h));
    return buf;
}

// Parse input-file lines "NNN XX" (hex byte per frame). Returns empty on failure.
static std::vector<uint8_t> load_input_script(const std::string& path, std::string& err) {
    std::vector<uint8_t> out;
    if (path.empty()) return out;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) { err = "cannot open input-file " + path; return out; }
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        // trim leading ws
        char* p = line;
        while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p;
        if (*p=='\0' || *p=='#') continue;
        // find last token (hex)
        char* end = p + std::strlen(p);
        while (end>p && (end[-1]=='\n'||end[-1]=='\r'||end[-1]==' '||end[-1]=='\t')) --end;
        *end='\0';
        char* last_sp = std::strrchr(p, ' ');
        char* hex = last_sp ? last_sp+1 : p;
        while (*hex==' '||*hex=='\t') ++hex;
        if (*hex=='\0') continue;
        // skip frame number token case: if we had "000 00", last_sp points to " 00"
        // but if line is "060 01", hex="01"
        char* e = nullptr;
        long v = std::strtol(hex, &e, 16);
        if (e==hex || v<0 || v>255) {
            // try decimal fallback
            v = std::strtol(hex, &e, 10);
            if (e==hex) continue;
        }
        out.push_back(uint8_t(v & 0xFF));
    }
    std::fclose(f);
    return out;
}



// ---------------------------------------------------------------------------
// PPU machine: scanline/dot counters, vblank flag, loopy latch registers.
// Rendering is delegated to nes22prio::render_frame once per completed
// frame (the homebrew writes scroll only during vblank, so the renderer's
// documented "no mid-frame register writes" model is exact for it).
// ---------------------------------------------------------------------------
struct Ppu {
    nes22scroll::Loopy l;                 // $2005/$2006/$2000 latches
    int scanline = 0, dot = 0;
    uint64_t frames = 0;
    bool vblank = false, prev_vblank = false;
    bool nmi_raise = false;

    void tick() {
        if (scanline == 241 && dot == 0) {
            vblank = true;
            if (!prev_vblank) nmi_raise = true;
        }
        ++dot;
        if (dot >= 341) {
            dot = 0;
            ++scanline;
            if (scanline >= 262) {
                scanline = 0;
                ++frames;
            }
        }
    }

    void end_of_step() { prev_vblank = vblank; }
};

// ---------------------------------------------------------------------------
// The whole machine bus: CPU RAM, PPU regs, APU regs, cartridge, DMA.
// ---------------------------------------------------------------------------
struct GateBus final : nes6502::Bus {
    nesrom::NROM* cart = nullptr;         // non-owning (runner owns)
    nes6502::Cpu* cpu = nullptr;          // non-owning (NMI clearing)
    std::array<uint8_t, 0x800> ram{};
    std::array<uint8_t, 0x20> palette{};
    std::array<uint8_t, 0x100> oam{};
    Ppu ppu;
    nes24sync::ApuLite apu;

    uint8_t ppuctrl = 0, ppumask = 0;
    std::vector<int16_t> audio;
    size_t audio_start = 0;  // kept for compatibility; whole-run hash below

    // OAM DMA state (ch24 contract).
    bool dma_pending = false, dma_active = false;
    uint8_t dma_page = 0;
    int dma_elapsed = 0, dma_dummy = 0;

    // Controller input (ch52 input-reactive fixture).
    std::vector<uint8_t> input_script;
    uint8_t pad_buttons = 0;
    uint8_t pad_latch = 0;
    bool pad_strobe = false;

    void set_input_script(const std::vector<uint8_t>& s) {
        input_script = s;
        pad_buttons = input_script.empty() ? 0 : input_script[0];
        pad_latch = pad_buttons;
    }
    void on_frame_boundary() {
        if (!input_script.empty()) {
            pad_buttons = input_script[ppu.frames % input_script.size()];
        }
    }

    uint8_t read(uint16_t addr) override {
        if (addr < 0x2000) {
            return ram[addr & 0x7FF];
        }
        if (addr < 0x4000) {
            const int reg = addr & 7;
            if (reg == 2) {  // $2002 status
                ppu.l.w = false;  // read clears the write toggle
                const uint8_t st = uint8_t(ppu.vblank ? 0x80 : 0);
                ppu.vblank = false;
                if (cpu) nes6502::set_nmi_line(*cpu, false);
                return st;
            }
            return 0;  // other PPU regs are write-only in this model
        }
        if (addr == 0x4015) {
            return uint8_t((apu.pulse1.enabled ? 1 : 0) |
                           (apu.pulse2.enabled ? 2 : 0) |
                           (apu.triangle.enabled ? 4 : 0) |
                           (apu.noise.enabled ? 8 : 0) |
                           (apu.dmc.enabled ? 16 : 0));
        }
        if (addr == 0x4016 || addr == 0x4017) {
            uint8_t v;
            if (pad_strobe) {
                v = uint8_t((pad_buttons & 1) | 0x40);
            } else {
                v = uint8_t((pad_latch & 1) | 0x40);
                pad_latch >>= 1;
                pad_latch |= 0x80;  // after 8 reads it stays 1 (open bus)
            }
            if (addr == 0x4017) v = uint8_t(0x40); // second controller always 0 + open bus
            // For $4016 with pad_strobe, keep latch refreshed; for $4017 always 0
            return (addr == 0x4016) ? v : uint8_t(0x40);
        }
        if (addr >= 0x8000 && cart) {
            return cart->cpu_read(addr);
        }
        return 0;
    }

    void write(uint16_t addr, uint8_t v) override {
        if (addr < 0x2000) {
            ram[addr & 0x7FF] = v;
        } else if (addr < 0x4000) {
            switch (addr & 7) {
                case 0: ppuctrl = v;
                    nes22scroll::ctrl_write(ppu.l, v);
                    break;
                case 1: ppumask = v; break;
                case 5: nes22scroll::scroll_write(ppu.l, v); break;
                case 6: nes22scroll::addr_write(ppu.l, v); break;
                case 7: {  // $2007 data write: loopy v selects the port
                    const uint16_t va = ppu.l.v;
                    if (va < 0x2000) {
                        cart->ppu_write(va, v);  // CHR (ROM: dropped)
                    } else if (va < 0x3F00) {
                        cart->ppu_write(va, v);  // nametable RAM
                    } else {
                        palette[va & 0x1F] = v;
                    }
                    ppu.l.v = uint16_t(ppu.l.v + 1);  // $2007 auto-increment
                    break;
                }
                default: break;
            }
        } else if (addr == 0x4014) {
            dma_page = v;
            dma_pending = true;
        } else if (addr == 0x4016) {
            bool new_strobe = (v & 1) != 0;
            if (new_strobe) pad_latch = pad_buttons;
            pad_strobe = new_strobe;
        } else if (addr == 0x4017) {
            // second controller strobe ignored (always 0) — also APU frame counter
            apu.write_reg(addr, v);
        } else if (addr == 0x4015 || (addr >= 0x4000 && addr <= 0x4013)) {
            apu.write_reg(addr, v);
        } else if (addr >= 0x8000 && cart) {
            cart->cpu_write(addr, v);  // NROM: dropped
        }
    }

    // One full CPU cycle (ch24 cpu_tick order: DMA -> PPU -> APU -> audio).
    void machine_tick() {
        if (!dma_active && dma_pending) {
            dma_pending = false;
            dma_active = true;
            dma_elapsed = 0;
            dma_dummy = (cpu->cycles & 1) ? 2 : 1;  // alignment rule
        }
        if (dma_active) {
            ++dma_elapsed;
            if (dma_elapsed > dma_dummy &&
                ((dma_elapsed - dma_dummy) & 1) == 0) {
                const int i = (dma_elapsed - dma_dummy) / 2 - 1;
                if (i >= 0 && i < 256)
                    oam[size_t(i)] =
                        ram[(uint16_t(dma_page) << 8) | uint8_t(i)];
            }
            if (dma_elapsed >= dma_dummy + 512) dma_active = false;
        }
        const int q0 = apu.frame.quarters, h0 = apu.frame.halves;
        const uint64_t frames_before = ppu.frames;
        for (int i = 0; i < kPpuDotsPerCpu; ++i) ppu.tick();
        if (ppu.frames != frames_before) on_frame_boundary();
        apu.tick_devices();
        audio.push_back(int16_t(apu.mix() * 512));
        if (apu.frame.quarters != q0) apu.quarter();
        if (apu.frame.halves != h0) apu.half();
    }

    // Render the completed frame through the ch22 course renderer.
    void render_frame(std::vector<uint8_t>& fb) {
        nes22prio::Scene s;
        s.mirroring = (cart->mirroring == nesrom::Mirroring::Vertical)
                          ? nes22prio::Mirroring::Vertical
                          : nes22prio::Mirroring::Horizontal;
        s.chr = cart->chr.data();
        s.nt = cart->ciram.data();
        s.pal = palette.data();
        s.oam = oam.data();
        s.l = ppu.l;
        s.ctrl = ppuctrl;
        s.mask = ppumask;
        std::array<uint8_t, kFbBytes> fbuf{};
        nes22prio::render_frame(fbuf, s);
        fb.assign(fbuf.begin(), fbuf.end());
    }

    // EMU_GATE_V1 checkpoint for the frames just executed.
    std::string checkpoint(const nes6502::Cpu& c, uint64_t frames_run,
                           const std::vector<uint8_t>& fb,
                           const std::string& rom_fn) const {
        char line[256];
        std::string out = "EMU_GATE_V1\n";
        auto add = [&](const char* k, const std::string& v) {
            out += k; out += '='; out += v; out += '\n';
        };
        add("ROM_FNV", rom_fn);
        std::snprintf(line, sizeof line, "%llu",
                      static_cast<unsigned long long>(frames_run));
        add("FRAME", line);
        std::snprintf(line, sizeof line, "%04X", c.pc);
        add("CPU_PC", line);
        std::snprintf(line, sizeof line, "%02X", c.a);
        add("CPU_A", line);
        std::snprintf(line, sizeof line, "%02X", c.x);
        add("CPU_X", line);
        std::snprintf(line, sizeof line, "%02X", c.y);
        add("CPU_Y", line);
        std::snprintf(line, sizeof line, "%02X", c.sp);
        add("CPU_SP", line);
        std::snprintf(line, sizeof line, "%02X", c.p);
        add("CPU_P", line);
        std::snprintf(line, sizeof line, "%llu",
                      static_cast<unsigned long long>(c.cycles));
        add("CPU_CYC", line);
        add("RAM_FNV", fnv_hex(fnv1a64(ram.data(), ram.size())));

        // PPU state: ctrl, mask, loopy (v, t, x, w), scanline, dot, then
        // nametable RAM (2 KiB), palette (32 B), OAM (256 B).
        std::vector<uint8_t> ppu_state;
        ppu_state.reserve(2 + 5 + 4 + 0x800 + 0x20 + 0x100);
        ppu_state.push_back(ppuctrl);
        ppu_state.push_back(ppumask);
        const uint16_t v = ppu.l.v, t = ppu.l.t;
        ppu_state.push_back(uint8_t(v >> 8));
        ppu_state.push_back(uint8_t(v & 0xFF));
        ppu_state.push_back(uint8_t(t >> 8));
        ppu_state.push_back(uint8_t(t & 0xFF));
        ppu_state.push_back(ppu.l.x);
        ppu_state.push_back(ppu.l.w ? 1 : 0);
        ppu_state.push_back(uint8_t(ppu.scanline));
        ppu_state.push_back(uint8_t(ppu.dot));
        for (uint8_t b : cart->ciram) ppu_state.push_back(b);
        for (uint8_t b : palette) ppu_state.push_back(b);
        for (uint8_t b : oam) ppu_state.push_back(b);
        add("PPU_FNV", fnv_hex(fnv1a64(ppu_state.data(), ppu_state.size())));
        add("FRAME_FNV", fnv_hex(fnv1a64(fb.data(), fb.size())));
        // Whole-run audio (one sample per CPU cycle; ch24 contract) — the
        // audio-out file hashes the identical bytes, so the two match.
        add("AUDIO_FNV",
            fnv_hex(fnv1a64(reinterpret_cast<const uint8_t*>(audio.data()),
                            audio.size() * 2)));
        add("REPLAY_FNV", "");
        return out;
    }
};
// ---------------------------------------------------------------------------
// Driver.
// ---------------------------------------------------------------------------
struct RunResult {
    std::string checkpoint;
    std::vector<uint8_t> frame;
    std::vector<int16_t> audio;
    uint64_t cycles = 0;
};

bool run_rom(const std::vector<uint8_t>& rom, uint64_t frames,
             GateBus& bus, RunResult& out, std::string& err) {
    nesrom::Header h;
    if (!h.parse(rom)) {
        err = "bad iNES header";
        return false;
    }
    nesrom::NROM* cart = nesrom::NROM::create(h, rom);
    if (!cart) {
        err = "cartridge create failed";
        return false;
    }
    bus.cart = cart;

    nes6502::Cpu cpu;
    cpu.bus = &bus;
    bus.cpu = &cpu;
    nes6502::reset(cpu);

    while (bus.ppu.frames < frames) {
        const uint64_t f0 = bus.ppu.frames;
        const int n = nes6502::step(cpu);
        for (int i = 0; i < n; ++i) bus.machine_tick();
        if (bus.ppu.nmi_raise && (bus.ppuctrl & 0x80)) {
            nes6502::set_nmi_line(cpu, true);
        }
        bus.ppu.nmi_raise = false;
        bus.ppu.end_of_step();
        if (cpu.halted) {
            err = "cpu halted mid-run";
            delete cart;
            return false;
        }
        (void)f0;
    }

    // Final render + checkpoint after the requested frame count.
    bus.render_frame(out.frame);
    const uint64_t rom_fn =
        fnv1a64(reinterpret_cast<const uint8_t*>(rom.data() + 16),
                rom.size() - 16);
    out.checkpoint =
        bus.checkpoint(cpu, frames, out.frame, fnv_hex(rom_fn));
    out.audio.assign(bus.audio.begin(), bus.audio.end());
    out.cycles = cpu.cycles;
    delete cart;
    bus.cart = nullptr;
    return true;
}

int run_cli(int argc, char** argv) {
    std::string rom_path, input_path, gate_path, hash_path, audio_path;
    uint64_t frames = 180;
    bool selfcheck = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--rom") rom_path = need("--rom");
        else if (a == "--input-file") input_path = need("--input-file");
        else if (a == "--frames") frames = std::strtoull(need("--frames"), nullptr, 10);
        else if (a == "--gate") gate_path = need("--gate");
        else if (a == "--hash-frame") hash_path = need("--hash-frame");
        else if (a == "--audio-out") audio_path = need("--audio-out");
        else if (a == "--headless") {}
        else if (a == "--selfcheck-replay") selfcheck = true;
        else {
            std::fprintf(stderr, "error: unknown flag '%s'\n", a.c_str());
            return 2;
        }
    }
    if (selfcheck && rom_path.empty()) {
        rom_path = "tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes";
    }
    if (rom_path.empty()) {
        std::fprintf(stderr, "error: --rom is required\n");
        return 2;
    }
    // Load the ROM once.
    std::vector<uint8_t> rom;
    if (!rom_path.empty()) {
        FILE* f = std::fopen(rom_path.c_str(), "rb");
        if (!f) {
            std::fprintf(stderr, "error: cannot open '%s'\n",
                         rom_path.c_str());
            return 1;
        }
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        rom.resize(size_t(sz));
        if (sz > 0 && std::fread(rom.data(), 1, size_t(sz), f) != size_t(sz)) {
            std::fclose(f);
            std::fprintf(stderr, "error: read failed '%s'\n",
                         rom_path.c_str());
            return 1;
        }
        std::fclose(f);
    }
    // Load input script if given (each line "NNN XX" hex, one byte per frame).
    std::vector<uint8_t> input_script;
    std::string input_err;
    if (!input_path.empty()) {
        input_script = load_input_script(input_path, input_err);
        if (!input_err.empty()) {
            std::fprintf(stderr, "error: %s\n", input_err.c_str());
            return 1;
        }
        if (input_script.empty()) {
            std::fprintf(stderr, "warn: input-file '%s' produced 0 entries (treated as all-zero)\n", input_path.c_str());
        }
    }

    auto write_file = [](const char* path, const void* data, size_t n) -> bool {
        FILE* f = std::fopen(path, "wb");
        if (!f) return false;
        const bool ok = n == 0 || std::fwrite(data, 1, n, f) == n;
        std::fclose(f);
        return ok;
    };

    if (selfcheck) {
        RunResult r1, r2;
        GateBus b1, b2;
        if (!input_script.empty()) { b1.set_input_script(input_script); b2.set_input_script(input_script); }
        std::string err;
        if (!run_rom(rom, frames, b1, r1, err) ||
            !run_rom(rom, frames, b2, r2, err)) {
            std::fprintf(stderr, "error: replay run failed: %s\n",
                         err.c_str());
            return 1;
        }
        // Compare every checkpoint field except REPLAY_FNV.
        auto body_of = [](const std::string& cp) {
            std::string body;
            size_t pos = 0;
            while ((pos = cp.find("REPLAY_FNV=")) != std::string::npos) {
                body = cp.substr(0, pos);
                break;
            }
            return body;
        };
        const bool same = body_of(r1.checkpoint) == body_of(r2.checkpoint);
        if (!same) {
            std::fprintf(stderr,
                         "error: replay mismatch (nondeterministic run)\n");
            return 1;
        }
        if (!gate_path.empty() &&
            !write_file(gate_path.c_str(), r1.checkpoint.data(),
                        r1.checkpoint.size())) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         gate_path.c_str());
            return 1;
        }
        return 0;
    }

    RunResult r;
    GateBus bus;
    if (!input_script.empty()) bus.set_input_script(input_script);
    std::string err;
    if (!run_rom(rom, frames, bus, r, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    if (!hash_path.empty() &&
        !write_file(hash_path.c_str(), r.frame.data(), r.frame.size())) {
        std::fprintf(stderr, "error: cannot write '%s'\n", hash_path.c_str());
        return 1;
    }
    if (!audio_path.empty()) {
        std::vector<uint8_t> pcm;
        pcm.resize(r.audio.size() * 2);
        for (size_t i = 0; i < r.audio.size(); ++i) {
            const uint16_t u = uint16_t(r.audio[i]);
            pcm[i * 2] = uint8_t(u & 0xFF);
            pcm[i * 2 + 1] = uint8_t(u >> 8);
        }
        if (!write_file(audio_path.c_str(), pcm.data(), pcm.size())) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         audio_path.c_str());
            return 1;
        }
    }
    if (!gate_path.empty() &&
        !write_file(gate_path.c_str(), r.checkpoint.data(),
                    r.checkpoint.size())) {
        std::fprintf(stderr, "error: cannot write '%s'\n", gate_path.c_str());
        return 1;
    }
    std::fprintf(stderr, "nes_gate: %llu frames, %llu cycles, %zu samples\n",
                 static_cast<unsigned long long>(frames),
                 static_cast<unsigned long long>(r.cycles),
                 r.audio.size());
    return 0;
}

}  // namespace nesgate

int main(int argc, char** argv) { return nesgate::run_cli(argc, argv); }
