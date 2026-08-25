//
// ch40 / 99_coding_test — unseen timer configuration, exact IRQ order
//
// CODING_TEST.md defines the fixture format. The provided infrastructure
// (irq.hpp / timers.hpp) is the chapter's verified reference; your job is
// the three @LABS-marked pieces that turn a raw configuration image into a
// deterministic interrupt-delivery log:
//
//   fixture.bin -> load_config() -> apply_config() -> run -> irq order log
//
// The hidden grader compares the FNV-1a-64 hash of your log against a
// golden produced by this reference solution.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "irq.hpp"
#include "scheduler.hpp"
#include "timers.hpp"

namespace {

constexpr uint32_t kMagic = 0x4D495443u;   // "CTIM" little-endian

// Synthetic video timing, identical to the rest of ch40.
constexpr uint64_t kHblankPeriod = 200;
constexpr uint64_t kHblankWidth = 40;
constexpr uint64_t kVblankPeriod = kHblankPeriod * 25;
constexpr uint64_t kVblankWidth = 400;

bool hblank_level(uint64_t c) { return c % kHblankPeriod < kHblankWidth; }
bool vblank_level(uint64_t c) { return c % kVblankPeriod < kVblankWidth; }
bool hblank_pulse(uint64_t c) { return c % kHblankPeriod == 0; }

struct Config {
    uint32_t run_cycles = 0;
    struct Entry {
        int timer;
        uint16_t mode;
        uint16_t target;
    };
    std::vector<Entry> entries;
};

uint64_t fnv64(const std::string& data) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (unsigned char b : data) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
bool load_config(const std::vector<uint32_t>& words, Config* out) {
    if (words.size() < 2 || words[0] != kMagic) return false;
    out->run_cycles = words[1];
    size_t i = 2;
    while (i + 2 < words.size()) {
        const int timer = static_cast<int32_t>(words[i]);
        if (timer < 0) break;                       // 0xFFFFFFFF terminator
        if (timer >= ps1::sysdev::kTimerCount) return false;
        out->entries.push_back(
            {timer, static_cast<uint16_t>(words[i + 1] & 0x1FFFu),
             static_cast<uint16_t>(words[i + 2])});
        i += 3;
    }
    return true;
}
//@LABS-STUB
bool load_config(const std::vector<uint32_t>& words, Config* out) {
    // TODO(1): validate the CTIM magic, read the run-cycle count from
    // word[1], then collect {timer, mode, target} triples until the
    // 0xFFFFFFFF terminator. Return false on any malformed input.
    (void)words;
    out->run_cycles = words[1];   // naive: skips validation and entries
    return true;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
void apply_config(ps1::sysdev::TimerBank& timers, const Config& cfg) {
    for (const auto& e : cfg.entries) {
        timers.write_target(e.timer, e.target);     // TARGET before MODE:
        timers.write_mode(e.timer, e.mode);         // MODE write starts it
    }
}
//@LABS-STUB
void apply_config(ps1::sysdev::TimerBank& timers, const Config& cfg) {
    // TODO(2): program each entry's TARGET first, then its MODE (the MODE
    // write clears the counter and starts the root counter).
    (void)timers;
    (void)cfg;
}
//@LABS-END

struct Rig {
    ps1::sysdev::IrqController irq;
    ps1::sysdev::TimerBank timers;
    ps1::sysdev::Scheduler sched;
    std::string log;
    bool started = false;

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    static void tick_cb(void* self) {
        auto* rig = static_cast<Rig*>(self);
        const uint64_t c = rig->sched.now();
        ps1::sysdev::TimerSignals s;
        s.hblank_pulse = hblank_pulse(c);
        s.hblank_level = hblank_level(c);
        s.vblank_level = vblank_level(c);
        // dotclock pulse every 6th cycle, like the chapter machine
        s.dot_pulse = (c % 6 == 0);
        rig->timers.tick(s, &on_irq, rig);
        rig->sched.schedule(c + 1, 1, &tick_cb, rig);   // keep the clock running
    }
    //@LABS-STUB
    static void tick_cb(void* self) {
        // TODO(3): sample this cycle's video/dot signals, advance all root
        // counters through the sink, and RESCHEDULE yourself for the next
        // cycle — without rescheduling the counters never reach their
        // targets.
        (void)self;
    }
    //@LABS-END

    void ensure_started(uint64_t from) {
        if (!started) {
            started = true;
            sched.schedule(from + 1, 1, &tick_cb, this);
        }
    }

    static void on_irq(void* self, int timer, bool asserted) {
        static constexpr uint32_t kTimerLine[3] = {
            ps1::sysdev::kIrqTimer0, ps1::sysdev::kIrqTimer1,
            ps1::sysdev::kIrqTimer2};
        auto* rig = static_cast<Rig*>(self);
        char line[48];
        if (asserted) {
            std::snprintf(line, sizeof(line), "irq=%04X cyc=%llu\n",
                          kTimerLine[timer],
                          static_cast<unsigned long long>(rig->sched.now()));
            rig->log += line;
        }
        if (asserted)
            rig->irq.raise(kTimerLine[timer]);
        else
            rig->irq.lower(kTimerLine[timer]);
    }
};

void usage(std::FILE* out) {
    std::fprintf(out,
                 "usage: ch40_99_coding_test_runner --rom PATH [--cycles N] "
                 "[--headless] [--trace FILE] [--hash-frame FILE]\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path, hash_path;
    bool have_rom = false;
    uint64_t cycle_override = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            usage(stdout);
            return 0;
        } else if (arg == "--rom" && i + 1 < argc) {
            rom_path = argv[++i];
            have_rom = true;
        } else if (arg == "--cycles" && i + 1 < argc) {
            cycle_override = std::strtoull(argv[++i], nullptr, 0);
        } else if (arg == "--headless") {
            // accepted, no-op
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--hash-frame" && i + 1 < argc) {
            hash_path = argv[++i];
        } else {
            usage(stderr);
            return 1;
        }
    }
    if (!have_rom) {
        usage(stderr);
        return 1;
    }

    FILE* rom = std::fopen(rom_path.c_str(), "rb");
    if (!rom) {
        std::fprintf(stderr, "cannot open rom: %s\n", rom_path.c_str());
        return 1;
    }
    std::vector<uint32_t> words;
    uint8_t quad[4];
    while (std::fread(quad, 1, 4, rom) == 4) {
        words.push_back(uint32_t{quad[0]} | uint32_t{quad[1]} << 8 |
                        uint32_t{quad[2]} << 16 | uint32_t{quad[3]} << 24);
    }
    std::fclose(rom);

    Config cfg;
    if (!load_config(words, &cfg)) {
        std::fprintf(stderr, "malformed config image\n");
        return 1;
    }
    if (cycle_override != 0) cfg.run_cycles = cycle_override;

    Rig rig;
    apply_config(rig.timers, cfg);

    // One scheduler pass per system cycle; events deliver in deterministic
    // (cycle, insertion) order.
    for (uint64_t c = 0; c < cfg.run_cycles; ++c) {
        rig.ensure_started(c);
        rig.sched.run_to(c + 1);
    }

    if (!trace_path.empty()) {
        FILE* f = std::fopen(trace_path.c_str(), "wb");
        if (!f) return 1;
        std::fwrite(rig.log.data(), 1, rig.log.size(), f);
        std::fclose(f);
    }
    if (!hash_path.empty()) {
        char payload[32];
        std::snprintf(payload, sizeof(payload), "fnv64=%016llX\n",
                      static_cast<unsigned long long>(fnv64(rig.log)));
        FILE* f = std::fopen(hash_path.c_str(), "wb");
        if (!f) return 1;
        std::fwrite(payload, 1, std::strlen(payload), f);
        std::fclose(f);
    }
    return 0;
}
