// Headless APU runner for exercise 04.
//
// Mandatory flag shape (docs/AUTHORING.md):
//   --rom PATH --headless --cycles N --frames N --trace FILE
//   --hash-frame FILE --input-file FILE
// Chapter extension:
//   --audio-out FILE   raw s16le interleaved stereo PCM dump
//
// `--rom` loads a .apuprog program: little-endian records of
//   u32 tcycleOffset, u16 regAddr, u8 value        (7 bytes each)
// terminated by a record whose regAddr == 0xFFFF. Addresses must fall in
// FF10-FF3F. The simulation runs total = max(--cycles, frames * 70224)
// T-cycles, applying each record at its offset from cycle 0.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstddef>

#include "apu.hpp"
#include "audio_ring.hpp"

namespace {

struct ProgEvent {
    uint32_t t;
    uint16_t addr;
    uint8_t val;
};

void usage() {
    std::printf(
        "ch17_04_apu_runner — headless DMG APU simulator\n"
        "usage: ch17_04_apu_runner --rom PROGRAM.apuprog [--frames N]\n"
        "       [--cycles N] [--audio-out OUT.pcm] [--hash-frame OUT.pcm]\n"
        "       [--trace FILE] [--headless] [--input-file FILE]\n"
        "\n"
        "extensions over the common CLI:\n"
        "  --audio-out FILE  drains the mixed s16le stereo ring buffer\n"
        "                    to FILE (same bytes --hash-frame writes)\n"
        "\n"
        ".apuprog format: records of u32 tcycleOffset, u16 regAddr,\n"
        "u8 value; terminator record has regAddr == 0xFFFF.\n");
}

bool loadProgram(const std::string& path, std::vector<ProgEvent>& events) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0 || size % 7 != 0) {
        std::fclose(f);
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    const size_t got = bytes.empty()
                           ? 0
                           : std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (got != bytes.size()) return false;

    for (size_t off = 0; off + 7 <= bytes.size(); off += 7) {
        const uint32_t t = static_cast<uint32_t>(bytes[off]) |
                           (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                           (static_cast<uint32_t>(bytes[off + 2]) << 16) |
                           (static_cast<uint32_t>(bytes[off + 3]) << 24);
        const uint16_t addr = static_cast<uint16_t>(
            bytes[off + 4] | (bytes[off + 5] << 8));
        const uint8_t val = bytes[off + 6];
        if (addr == 0xFFFF) return true;  // terminator
        if (addr < 0xFF10 || addr > 0xFF3F) {
            std::fprintf(stderr, "bad register address %04X in program\n",
                         addr);
            return false;
        }
        events.push_back({t, addr, val});
    }
    std::fprintf(stderr, "missing FFFF terminator record\n");
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::string romPath, audioOut, hashPath, tracePath;
    long long cycles = -1;
    int frames = -1;
    bool headless = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--rom") romPath = next();
        else if (a == "--audio-out") audioOut = next();
        else if (a == "--hash-frame") hashPath = next();
        else if (a == "--trace") tracePath = next();
        else if (a == "--cycles") cycles = std::atoll(next());
        else if (a == "--frames") frames = std::atoi(next());
        else if (a == "--headless") headless = true;
        else if (a == "--input-file") ++i;  // accepted for CLI parity
        else {
            std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
            usage();
            return 2;
        }
    }
    (void)headless;
    if (romPath.empty()) { usage(); return 2; }
    if (cycles < 0 && frames < 0) {
        std::fprintf(stderr, "give --cycles or --frames\n");
        return 2;
    }

    std::vector<ProgEvent> events;
    if (!loadProgram(romPath, events)) {
        std::fprintf(stderr, "bad or missing program: %s\n",
                     romPath.c_str());
        return 1;
    }
    std::stable_sort(events.begin(), events.end(),
                     [](const ProgEvent& a, const ProgEvent& b) {
                         return a.t < b.t;
                     });

    const uint64_t frameCycles =
        frames > 0 ? static_cast<uint64_t>(frames) *
                         gbaudio::Apu::kFrameTcycles
                   : 0;
    uint64_t total = static_cast<uint64_t>(cycles > 0 ? cycles : 0);
    if (frameCycles > total) total = frameCycles;

    gbaudio::S16RingBuffer ring;
    gbaudio::Apu apu(ring);

    std::FILE* trace = tracePath.empty() ? nullptr
                                         : std::fopen(tracePath.c_str(), "w");
    size_t nextEvent = 0;
    for (uint64_t t = 0; t < total; ++t) {
        while (nextEvent < events.size() && events[nextEvent].t <= t) {
            const ProgEvent& e = events[nextEvent++];
            apu.writeReg(e.addr, e.val);
            if (trace)
                std::fprintf(trace, "t=%u reg=%04X val=%02X\n", e.t,
                             e.addr, e.val);
        }
        apu.tick();
    }
    if (trace) std::fclose(trace);

    auto drainToPath = [&](const std::string& path) -> bool {
        std::FILE* out = std::fopen(path.c_str(), "wb");
        if (!out) return false;
        const bool ok = ring.drainTo(out);
        std::fclose(out);
        return ok;
    };
    if (!audioOut.empty() && !drainToPath(audioOut)) {
        std::fprintf(stderr, "cannot write audio-out: %s\n",
                     audioOut.c_str());
        return 1;
    }
    if (!hashPath.empty() && !drainToPath(hashPath)) {
        std::fprintf(stderr, "cannot write hash-frame: %s\n",
                     hashPath.c_str());
        return 1;
    }

    std::printf("simulated %llu T-cycles, emitted %zu samples "
                "(%d channel regs applied)\n",
                static_cast<unsigned long long>(total),
                ring.size(), static_cast<int>(nextEvent));
    return 0;
}
