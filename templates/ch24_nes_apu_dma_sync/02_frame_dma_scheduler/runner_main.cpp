// Headless sync runner for Chapter 24 (mandatory CLI shape, §52):
//
//   ch24_04_sync_runner --rom SCRIPT --frames N --headless
//                       [--cycles M] [--trace FILE] [--hash-frame FILE]
//                       [--audio-out FILE] [--input-file FILE]
//
// `--rom` loads an op script (course-original grammar, '#' comments):
//
//   pal <idx> <val>        palette[i] = val
//   chrpat <tile> <lo> <hi>  tile planes filled with the two bytes
//   nt <idx> <val>         nametable RAM[idx] = val
//   wr <hexaddr> <hexval>  CPU store ($2000/$2001/$2005/$40xx/...)
//   frame [n]              run n frames (default 1)
//
// Deterministic, integer-only. Audio: one mono s16le sample per CPU cycle.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "machine.hpp"

namespace {
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;

uint64_t fnv(const void* data, size_t n) {
    uint64_t h = kFnvOffset;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= kFnvPrime;
    }
    return h;
}
}  // namespace

int main(int argc, char** argv) {
    std::string rom, trace_path, hash_path, audio_path;
    long frames = -1, cycle_cap = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help") {
            std::printf(
                "usage: ch24_04_sync_runner --rom SCRIPT [--frames N]\n"
                "       [--headless] [--cycles M] [--trace FILE]\n"
                "       [--hash-frame FILE] [--audio-out FILE]\n"
                "       [--input-file FILE]\n");
            return 0;
        } else if (a == "--rom") rom = next();
        else if (a == "--frames") frames = atol(next());
        else if (a == "--cycles") cycle_cap = atol(next());
        else if (a == "--trace") trace_path = next();
        else if (a == "--hash-frame") hash_path = next();
        else if (a == "--audio-out") audio_path = next();
        else if (a == "--headless" || a == "--input-file") { if (a != "--headless") next(); }
        else { std::fprintf(stderr, "unknown flag %s\n", a.c_str()); return 2; }
    }
    FILE* sf = fopen(rom.c_str(), "r");
    if (!sf) { std::fprintf(stderr, "cannot open script %s\n", rom.c_str()); return 2; }

    nes24sync::Machine m;
    std::string trace;
    char line[256], buf[96];

    // Pass 1: apply all setup ops up to the first 'frame'; then interleave.
    while (frames > 0 && fgets(line, sizeof(line), sf)) {
        char op[16];
        unsigned a1 = 0, a2 = 0, a3 = 0;
        long n = 1;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%15s", op) != 1) continue;
        if (strcmp(op, "pal") == 0 &&
            sscanf(line, "%*s %x %x", &a1, &a2) == 2)
            m.ppu.palette[a1 & 31] = uint8_t(a2);
        else if (strcmp(op, "chrpat") == 0 &&
                 sscanf(line, "%*s %x %x %x", &a1, &a2, &a3) == 3)
            for (int k = 0; k < 8; ++k) {
                m.ppu.chr[(a1 * 16 + k) & 0x1FFF] = uint8_t(a2);
                m.ppu.chr[(a1 * 16 + 8 + k) & 0x1FFF] = uint8_t(a3);
            }
        else if (strcmp(op, "nt") == 0 &&
                 sscanf(line, "%*s %x %x", &a1, &a2) == 2)
            m.ppu.vram[a1 & 0x7FF] = uint8_t(a2);
        else if (strcmp(op, "wr") == 0 &&
                 sscanf(line, "%*s %x %x", &a1, &a2) == 2)
            m.cpu_write(uint16_t(a1), uint8_t(a2));
        else if (strcmp(op, "frame") == 0) {
            sscanf(line, "%*s %ld", &n);
            for (long f = 0; f < n && frames > 0; ++f, --frames) {
                m.run_one_frame();
                if (cycle_cap >= 0 &&
                    m.cpu_cycle >= uint64_t(cycle_cap)) { frames = 0; break; }
            }
            std::snprintf(buf, sizeof(buf),
                          "frame=%llu video=%016llX cyc=%llu irq=%d\n",
                          (unsigned long long)m.ppu.frames_done,
                          (unsigned long long)fnv(
                              m.ppu.last_frame_rgba.data(),
                              m.ppu.last_frame_rgba.size()),
                          (unsigned long long)m.cpu_cycle,
                          int(m.apu.frame.irq));
            trace += buf;
        }
    }
    fclose(sf);

    // Finalize last-frame hash + audio dump.
    uint64_t frame_hash =
        fnv(m.ppu.last_frame_rgba.data(), m.ppu.last_frame_rgba.size());
    uint64_t audio_hash = fnv(m.audio.data(),
                              m.audio.size() * sizeof(int16_t));
    if (!hash_path.empty()) {
        FILE* hf = fopen(hash_path.c_str(), "wb");
        if (!hf) return 2;
        fwrite(m.ppu.last_frame_rgba.data(), 1,
               m.ppu.last_frame_rgba.size(), hf);
        fclose(hf);
    }
    if (!audio_path.empty()) {
        FILE* af = fopen(audio_path.c_str(), "wb");
        if (!af) return 2;
        fwrite(m.audio.data(), sizeof(int16_t), m.audio.size(), af);
        fclose(af);
    }
    if (!trace_path.empty()) {
        FILE* tf = fopen(trace_path.c_str(), "wb");
        if (!tf) return 2;
        fwrite(trace.data(), 1, trace.size(), tf);
        fclose(tf);
    }
    std::printf("fnv64=%016llX\naudio_fnv64=%016llX\nframes=%llu\ncpu_cycles=%llu\n",
                (unsigned long long)frame_hash,
                (unsigned long long)audio_hash,
                (unsigned long long)m.ppu.frames_done,
                (unsigned long long)m.cpu_cycle);
    return 0;
}
