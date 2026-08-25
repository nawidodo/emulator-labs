// ch05 headless runner — the starter framebuffer backend.
//
//   ch05_06_hash_runner --rom FILE [--headless] [--cycles N | --frames N]
//       [--trace FILE] [--hash-frame FILE] [--frame-hashes FILE]
//       [--input-file FILE] [--beep-log FILE]
//
// Deterministic by construction: fixed 600 cycles/s, 60 Hz timers, scripted
// keypad feed. Same flags as every other chapter's runner (docs/AUTHORING.md).
#include "machine.hpp"
#include "frame_io.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int usage() {
    std::printf(
        "ch05 CHIP-8 graphics/input/timers runner\n"
        "usage: ch05_06_hash_runner --rom PATH [flags]\n"
        "\n"
        "  --rom PATH         ROM image, loaded at 0x200 (required)\n"
        "  --headless         accepted for CLI-shape parity; always headless\n"
        "  --cycles N         execute exactly N CPU cycles and exit\n"
        "  --frames N         execute N frames (N x %u cycles), one input line per frame\n"
        "  --input-file FILE  scripted keypad feed: one frame per line of held hex digits ('.'=none)\n"
        "  --trace FILE       per-instruction trace: pc=<hex> op=<hex> cyc=<n>\n"
        "  --hash-frame FILE  write final display as RGBA8888 (64x32x4 bytes)\n"
        "  --frame-hashes FILE write one FNV64 hex digest per executed frame\n"
        "  --beep-log FILE    record beep transitions with their frame numbers\n",
        chip8::kCyclesPerFrame);
    return 0;
}

bool read_file_bytes(const std::string& path, std::vector<uint8_t>* out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->clear();
    int c;
    while ((c = std::fgetc(f)) != EOF) out->push_back(uint8_t(c));
    std::fclose(f);
    return true;
}

std::string bytes_to_string(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

struct Options {
    std::string rom_path;
    bool has_cycles = false;
    bool has_frames = false;
    uint64_t cycles = 0;
    uint64_t frames = 0;
    std::string trace_path, hash_frame_path, frame_hashes_path,
        input_path, beep_log_path;
};

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        const auto next = [&]() -> const char* {
            return (k + 1 < argc) ? argv[++k] : nullptr;
        };
        const auto take_path = [&](std::string* dest) -> bool {
            const char* v = next();
            if (!v) return false;
            *dest = v;
            return true;
        };
        if (a == "--help" || a == "-h") return usage();
        else if (a == "--headless") { /* always headless */ }
        else if (a == "--rom") { if (!take_path(&opt.rom_path)) return usage(); }
        else if (a == "--cycles") {
            if (const char* v = next()) {
                opt.has_cycles = true;
                opt.cycles = std::strtoull(v, nullptr, 0);
            }
        } else if (a == "--frames") {
            if (const char* v = next()) {
                opt.has_frames = true;
                opt.frames = std::strtoull(v, nullptr, 0);
            }
        } else if (a == "--trace") { if (!take_path(&opt.trace_path)) return usage(); }
        else if (a == "--hash-frame") { if (!take_path(&opt.hash_frame_path)) return usage(); }
        else if (a == "--frame-hashes") { if (!take_path(&opt.frame_hashes_path)) return usage(); }
        else if (a == "--input-file") { if (!take_path(&opt.input_path)) return usage(); }
        else if (a == "--beep-log") { if (!take_path(&opt.beep_log_path)) return usage(); }
        else { std::fprintf(stderr, "unknown flag: %s\n", a.c_str()); return usage(); }
    }

    if (opt.rom_path.empty()) {
        std::fprintf(stderr, "error: --rom is required\n");
        return usage();
    }
    if (opt.has_cycles && opt.has_frames) {
        std::fprintf(stderr,
                     "error: --cycles and --frames are mutually exclusive\n");
        return 2;
    }

    std::vector<uint8_t> rom;
    if (!read_file_bytes(opt.rom_path, &rom)) {
        std::fprintf(stderr, "error: cannot read rom '%s'\n",
                     opt.rom_path.c_str());
        return 2;
    }

    chip8::Machine m;
    m.load(rom);

    chip8::InputFeed feed;
    if (!opt.input_path.empty()) {
        std::vector<uint8_t> in_bytes;
        if (!read_file_bytes(opt.input_path, &in_bytes)) {
            std::fprintf(stderr, "error: cannot read input file '%s'\n",
                         opt.input_path.c_str());
            return 2;
        }
        feed = chip8::InputFeed::parse(bytes_to_string(in_bytes));
    }

    std::FILE* trace = nullptr;
    if (!opt.trace_path.empty()) {
        trace = std::fopen(opt.trace_path.c_str(), "w");
        if (!trace) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         opt.trace_path.c_str());
            return 2;
        }
        // Course trace format; cyc counts instructions executed so far.
        m.on_step = [&](const chip8::Machine& mach, uint16_t op) {
            std::fprintf(trace, "pc=%03X op=%04X cyc=%llu\n", mach.pc, op,
                         static_cast<unsigned long long>(mach.steps_done));
        };
    }

    std::FILE* beep_log = nullptr;
    uint64_t elapsed_cycles = 0;
    if (!opt.beep_log_path.empty()) {
        beep_log = std::fopen(opt.beep_log_path.c_str(), "w");
        if (!beep_log) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         opt.beep_log_path.c_str());
            return 2;
        }
        m.timers.on_beep = [&](bool started) {
            std::fprintf(beep_log, "%s frame=%llu\n",
                         started ? "beep_start" : "beep_end",
                         static_cast<unsigned long long>(
                             elapsed_cycles / chip8::kCyclesPerFrame));
        };
    }

    const uint64_t total_cycles =
        opt.has_frames ? opt.frames * chip8::kCyclesPerFrame : opt.cycles;

    std::string frame_digests;
    uint64_t frames_executed = 0;
    while (elapsed_cycles < total_cycles) {
        const uint64_t remaining = total_cycles - elapsed_cycles;
        const uint64_t chunk =
            remaining < chip8::kCyclesPerFrame ? remaining : chip8::kCyclesPerFrame;
        feed.apply(m.keypad, frames_executed);
        m.run(chunk);
        elapsed_cycles += chunk;
        ++frames_executed;
        if (!opt.frame_hashes_path.empty())
            frame_digests += chip8::frame_hash_hex(m.display) + "\n";
    }

    if (!opt.frame_hashes_path.empty() &&
        !chip8::write_text_file(opt.frame_hashes_path, frame_digests)) {
        std::fprintf(stderr, "error: cannot write '%s'\n",
                     opt.frame_hashes_path.c_str());
        return 2;
    }
    if (!opt.hash_frame_path.empty()) {
        const std::vector<uint8_t> buf = chip8::frame_bytes(m.display);
        if (!chip8::write_byte_file(opt.hash_frame_path, buf.data(),
                                    buf.size())) {
            std::fprintf(stderr, "error: cannot write '%s'\n",
                         opt.hash_frame_path.c_str());
            return 2;
        }
    }
    if (trace) std::fclose(trace);
    if (beep_log) std::fclose(beep_log);

    std::printf("frames=%llu cycles=%llu pc=%03X frame_hash=%s\n",
                static_cast<unsigned long long>(elapsed_cycles / chip8::kCyclesPerFrame),
                static_cast<unsigned long long>(elapsed_cycles),
                m.pc, chip8::frame_hash_hex(m.display).c_str());
    return 0;
}
