// Headless runner for the ch06 tracing exercise (curriculum §52 shape).
//
//   ch06_01_trace_runner --rom PATH [--cycles N | --frames N]
//       [--trace FILE [--trace-full]] [--hash-frame FILE]
//       [--input-file FILE] [--quirks SPEC] [--headless]
//
// --quirks SPEC selects a profile by name (COSMAC_VIP, CHIP48, MODERN) or
// individual flags: vf_reset=0,shift_uses_vy=1,...
//
// Exit status is 0 unless a CLI/file error occurs. A single summary line
// "instr=<n> final_pc=%03X" goes to stdout.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstddef>

#include "chip8.hpp"
#include "trace.hpp"

namespace {

bool parse_u64(const char* s, uint64_t& out) {
    if (!s || !*s) return false;
    char* end = nullptr;
    out = std::strtoull(s, &end, 0);
    return end && *end == '\0';
}

const ch06::Chip8Quirks* quirks_by_name(const std::string& name,
                                        ch06::Chip8Quirks& storage) {
    if (name == "MODERN") return &ch06::kModernQuirks;
    if (name == "COSMAC_VIP") return &ch06::kCosmacVipQuirks;
    if (name == "CHIP48") {
        storage.vf_reset = false;
        storage.shift_uses_vy = true;
        storage.load_store_leaves_i = false;
        storage.wrapping = false;
        storage.jump_bnnn_x = true;
        return &storage;
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path, frame_path, input_path, quirks_spec;
    bool full = false;
    uint64_t cycles = 0, frames = 0;
    bool have_cycles = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need_value = [&](const char* flag, std::string& dst) -> bool {
            if (i + 1 >= argc) {
                std::cerr << "error: " << flag << " needs a value\n";
                return false;
            }
            dst = argv[++i];
            return true;
        };
        if (a == "--help" || a == "-h") {
            std::cout << "usage: ch06_01_trace_runner --rom PATH "
                         "[--cycles N|--frames N] [--trace FILE] "
                         "[--trace-full] [--hash-frame FILE] "
                         "[--input-file FILE] [--quirks SPEC] [--headless]\n";
            return 0;
        } else if (a == "--rom" && need_value("--rom", rom_path)) {
        } else if (a == "--trace" && need_value("--trace", trace_path)) {
        } else if (a == "--hash-frame" && need_value("--hash-frame", frame_path)) {
        } else if (a == "--input-file" && need_value("--input-file", input_path)) {
        } else if (a == "--quirks" && need_value("--quirks", quirks_spec)) {
        } else if (a == "--trace-full") {
            full = true;
        } else if (a == "--headless") {
            // accepted for CLI-shape compatibility; execution is always headless
        } else if (a == "--cycles" && !have_cycles) {
            have_cycles = parse_u64(argv[++i], cycles);
            if (!have_cycles) { std::cerr << "error: bad --cycles\n"; return 1; }
        } else if (a == "--frames") {
            if (!parse_u64(argv[++i], frames)) {
                std::cerr << "error: bad --frames\n";
                return 1;
            }
            cycles = frames * ch06::kInstrPerFrame;
            have_cycles = true;
        } else {
            std::cerr << "error: unknown argument: " << a << "\n";
            return 1;
        }
    }

    if (rom_path.empty()) {
        std::cerr << "error: --rom is required\n";
        return 1;
    }

    std::ifstream rf(rom_path, std::ios::binary);
    if (!rf) {
        std::cerr << "error: cannot open rom: " << rom_path << "\n";
        return 1;
    }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rf)),
                             std::istreambuf_iterator<char>());

    ch06::Chip8Quirks quirk_storage;
    const ch06::Chip8Quirks* quirks = &ch06::kModernQuirks;
    if (!quirks_spec.empty()) {
        quirks = quirks_by_name(quirks_spec, quirk_storage);
        if (!quirks) {
            std::cerr << "error: unknown quirks profile: " << quirks_spec << "\n";
            return 1;
        }
    }

    // Optional scripted keypad: line i holds hex key digits held during
    // frame i ("5A" = keys 5 and A down).
    std::vector<std::vector<bool>> key_frames;
    if (!input_path.empty()) {
        std::ifstream kf(input_path);
        if (!kf) { std::cerr << "error: cannot open " << input_path << "\n"; return 1; }
        std::string line;
        while (std::getline(kf, line)) {
            std::vector<bool> keys(16, false);
            for (char c : line) {
                int d = -1;
                if (c >= '0' && c <= '9') d = c - '0';
                if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                if (d >= 0) keys[d] = true;
            }
            key_frames.push_back(std::move(keys));
        }
    }

    ch06::Chip8 cpu;
    cpu.reset(*quirks);
    cpu.load(rom);

    std::ofstream tf;
    if (!trace_path.empty()) tf.open(trace_path, std::ios::binary);
    if (!trace_path.empty() && !tf) {
        std::cerr << "error: cannot open trace output: " << trace_path << "\n";
        return 1;
    }
    ch06::TraceWriter tw(tf, full);

    uint64_t ran = 0;
    while (ran < cycles && !cpu.halted()) {
        const size_t frame = static_cast<size_t>(ran / ch06::kInstrPerFrame);
        if (frame < key_frames.size())
            for (int k = 0; k < 16; ++k) cpu.keys[k] = key_frames[frame][k];
        if (tf) tw.log(cpu);
        cpu.step();
        ++ran;
    }

    if (!frame_path.empty()) {
        std::ofstream ff(frame_path, std::ios::binary);
        if (!ff) { std::cerr << "error: cannot write " << frame_path << "\n"; return 1; }
        for (bool pixel : cpu.fb) ff.put(pixel ? '\x01' : '\x00');
    }

    std::printf("instr=%llu final_pc=%03X\n",
                static_cast<unsigned long long>(ran), cpu.pc);
    return 0;
}
