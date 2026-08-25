// Headless SPU runner (curriculum §52 CLI shape).
//
//   ch47_05_spu_runner --rom adpcm.bin --input-file script.txt \
//       --frames 4000 --hash-frame out.pcm --trace events.log --headless
//
// The ROM is a raw PSX ADPCM stream DMA'd into SPU RAM at byte address
// 0x1000. The optional input file is a deterministic event script, one
// command per line:
//
//   VOL <voice> <voll> <volr>          voice volume registers
//   PITCH <voice> <pitch>              pitch register
//   ADSR <voice> <adsr1> <adsr2>       hex envelope registers
//   START <voice> <addr_word>          start address (>>3 units)
//   MAIN <l> <r>                       main volume
//   CDVOL <l> <r>                      CD input volume
//   CD <l> <r>                         fixed CD input sample value
//   IRQADDR <addr_word>                IRQ9 compare address
//   IRQON                              enable IRQ9 (control.6)
//   KEYON <mask_lo> [mid] [hi]         key on bitmask
//   KEYOFF <mask_lo> [mid] [hi]        key off bitmask
//   RENDER <frames>                    render frames to the PCM buffer
//
// Trace lines follow the canonical key=value shape with cyc=<n>; the SPU
// ticks at 44100 Hz and the CPU at 33.8688 MHz = exactly 768 cycles per
// SPU frame.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "spu.hpp"
#include "../shared/fnv.hpp"

namespace {

constexpr uint32_t kLoadAddr = 0x1000;
constexpr uint64_t kCpuCyclesPerFrame = 768;

uint64_t cpu_cycles(uint64_t frames) { return frames * kCpuCyclesPerFrame; }

bool load_rom(spu::Spu& spu, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<char> data((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    spu.dma_write(kLoadAddr,
                  std::span<const uint8_t>(
                      reinterpret_cast<const uint8_t*>(data.data()),
                      data.size()));
    return true;
}

uint16_t num16(const std::string& tok) {
    return static_cast<uint16_t>(std::stoul(tok, nullptr, 0));
}

void run_script(spu::Spu& spu, const std::string& path,
                std::vector<int16_t>& pcm, std::vector<std::string>& trace,
                uint64_t& total_frames) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "cannot open script: " << path << "\n";
        std::exit(2);
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string op;
        ss >> op;
        auto checkpoint = [&](int frames) {
            for (int p = 256; p <= frames; p += 256)
                trace.push_back("pos=" + std::to_string(total_frames + p) +
                                " cyc=" + std::to_string(cpu_cycles(
                                             total_frames + p)));
            total_frames += frames;
        };
        if (op == "VOL") {
            int v; std::string l, r;
            ss >> v >> l >> r;
            spu.write(0x10 * v + 0x00, num16(l));
            spu.write(0x10 * v + 0x02, num16(r));
        } else if (op == "PITCH") {
            int v; std::string p;
            ss >> v >> p;
            spu.write(0x10 * v + 0x04, num16(p));
        } else if (op == "ADSR") {
            int v; std::string a1, a2;
            ss >> v >> a1 >> a2;
            spu.write(0x10 * v + 0x08, num16(a1));
            spu.write(0x10 * v + 0x0A, num16(a2));
        } else if (op == "START") {
            int v; std::string a;
            ss >> v >> a;
            spu.write(0x10 * v + 0x06, num16(a));
        } else if (op == "MAIN") {
            std::string l, r;
            ss >> l >> r;
            spu.write(0x180, num16(l));
            spu.write(0x182, num16(r));
        } else if (op == "CDVOL") {
            std::string l, r;
            ss >> l >> r;
            spu.write(0x198, num16(l));
            spu.write(0x19A, num16(r));
        } else if (op == "CD") {
            std::string l, r;
            ss >> l >> r;
            spu.set_cd_input(static_cast<int16_t>(num16(l)),
                             static_cast<int16_t>(num16(r)));
        } else if (op == "IRQADDR") {
            std::string a;
            ss >> a;
            spu.write(0x1B4, num16(a));
        } else if (op == "IRQON") {
            uint16_t c = spu.read(0x1D8);
            spu.write(0x1D8, static_cast<uint16_t>(c | 0x40));
        } else if (op == "KEYON" || op == "KEYOFF") {
            std::string lo, mid = "0", hi = "0";
            ss >> lo >> mid >> hi;
            uint64_t mask = static_cast<uint64_t>(num16(lo)) |
                            (static_cast<uint64_t>(num16(mid)) << 16) |
                            (static_cast<uint64_t>(num16(hi)) << 32);
            trace.push_back(std::string("ev=") +
                            (op == "KEYON" ? "keyon" : "keyoff") +
                            " mask=" + [&] {
                                char buf[24];
                                std::snprintf(buf, sizeof buf, "%llx",
                                              (unsigned long long)mask);
                                return std::string(buf);
                            }() +
                            " cyc=" + std::to_string(cpu_cycles(total_frames)));
            if (op == "KEYON")
                spu.write(0x1C0, static_cast<uint16_t>(mask & 0xFFFF));
            else
                spu.write(0x1CC, static_cast<uint16_t>(mask & 0xFFFF));
            // mid/hi words when present
            if (op == "KEYON") {
                spu.write(0x1C1, static_cast<uint16_t>(mask >> 16));
                spu.write(0x1C2, static_cast<uint16_t>(mask >> 32));
            } else {
                spu.write(0x1CD, static_cast<uint16_t>(mask >> 16));
                spu.write(0x1CE, static_cast<uint16_t>(mask >> 32));
            }
        } else if (op == "RENDER") {
            int frames = 0;
            ss >> frames;
            spu.render(frames, pcm);
            checkpoint(frames);
        } else {
            std::cerr << "unknown script op: " << op << "\n";
            std::exit(2);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    spu::Spu spu;
    spu.reset();
    std::vector<int16_t> pcm;
    std::vector<std::string> trace;
    uint64_t total_frames = 0;

    std::string rom, script, hash_out, trace_out;
    long frames_arg = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom = next();
        else if (a == "--input-file") script = next();
        else if (a == "--hash-frame") hash_out = next();
        else if (a == "--trace") trace_out = next();
        else if (a == "--frames") frames_arg = std::stol(next());
        else if (a == "--headless") { /* accepted no-op */ }
        else if (a == "--help") {
            std::cout <<
                "usage: ch47_05_spu_runner [--rom FILE] [--input-file SCRIPT]"
                " [--frames N] [--hash-frame FILE] [--trace FILE] --headless\n";
            return 0;
        } else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }

    if (!rom.empty() && !load_rom(spu, rom)) {
        std::cerr << "cannot load rom: " << rom << "\n";
        return 2;
    }
    if (!script.empty()) run_script(spu, script, pcm, trace, total_frames);

    // Default render when no script asked for one explicitly.
    bool scripted_render =
        !script.empty();  // scripts always drive their own RENDER ops
    if (frames_arg >= 0 && !scripted_render) {
        spu.render(static_cast<int>(frames_arg), pcm);
        for (long p = 256; p <= frames_arg; p += 256)
            trace.push_back("pos=" + std::to_string(p) +
                            " cyc=" + std::to_string(cpu_cycles(p)));
    }

    if (!trace_out.empty()) {
        std::ofstream t(trace_out, std::ios::binary);
        for (const auto& line : trace) t << line << "\n";
    }
    if (!hash_out.empty()) {
        std::ofstream h(hash_out, std::ios::binary);
        h.write(reinterpret_cast<const char*>(pcm.data()),
                static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(pcm.data());
    std::printf("frames=%llu fnv64=%016llX irq=%d\n",
                (unsigned long long)(pcm.size() / 2),
                (unsigned long long)spu::fnv64(
                    std::span<const uint8_t>(bytes, pcm.size() * 2)),
                spu.irq_flag() ? 1 : 0);
    return 0;
}
