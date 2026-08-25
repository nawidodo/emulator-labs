// Headless SM83 timer-interrupt runner (curriculum §52 shape).
//
//   ch13_91_timer_runner --rom prog.bin --headless --cycles 200000 \
//       --trace irq.log --hash-frame state.txt
//
// The machine is the copied chapter CPU + TimerDevice + IntCtl glued via
// IntBus and timer_tick() (see ../04_interrupt_delivery/machine.hpp).
// --trace writes the INTERRUPT LOG (one line per event plus a final
// `state` line; exact format in format.hpp). --hash-frame writes the
// canonical final-state dump. In this phase --frames and --input-file are
// accepted and ignored (no PPU/joypad yet).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../04_interrupt_delivery/machine.hpp"
#include "format.hpp"

namespace {

struct Options {
    const char* rom = nullptr;
    long long cycles = 1000000;
    const char* trace = nullptr;
    const char* hash_frame = nullptr;
};

std::vector<uint8_t> read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(2);
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0) std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    return data;
}

void write_file(const char* path, const std::string& text) {
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", path);
        std::exit(2);
    }
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

void usage() {
    std::printf(
        "usage: ch13_91_timer_runner --rom PATH [--headless] "
        "[--cycles N] [--frames N]\n"
        "                           [--trace FILE] [--hash-frame FILE] "
        "[--input-file FILE]\n");
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            usage();
            return 0;
        }
        if (arg == "--rom" && i + 1 < argc) opt.rom = argv[++i];
        else if (arg == "--cycles" && i + 1 < argc) opt.cycles = std::atoll(argv[++i]);
        else if (arg == "--trace" && i + 1 < argc) opt.trace = argv[++i];
        else if (arg == "--hash-frame" && i + 1 < argc) opt.hash_frame = argv[++i];
        else if (arg == "--input-file" && i + 1 < argc) ++i;  // no keypad yet
        else if (arg == "--frames" && i + 1 < argc) ++i;      // no PPU yet
        else if (arg == "--headless") {}                      // always headless
        else {
            std::fprintf(stderr, "unknown/incomplete argument: %s\n", argv[i]);
            usage();
            return 2;
        }
    }
    if (!opt.rom) {
        std::fprintf(stderr, "--rom PATH is required\n");
        usage();
        return 2;
    }

    gb::TimerMachine m;
    m.load(read_file(opt.rom));  // vector-page image, base $0000

    std::string log;
    long overflows = 0;
    long irqs = 0;

    while (!m.cpu.trap && m.cyc < static_cast<uint64_t>(opt.cycles)) {
        if (m.cpu.halted && (m.ctl.flags & m.ctl.enabled) != 0)
            m.cpu.halted = false;  // wake: the divider keeps HALT alive
        const gb::StepReport rep = m.step_once();
        if (rep.overflow) {
            log += gbfmt::overflow_line(m.cyc);
            ++overflows;
        }
        if (rep.serviced) {
            log += gbfmt::irq_line(m.cyc, rep.vector, rep.ime_at_dispatch);
            ++irqs;
        }
    }

    log += gbfmt::state_line(m);
    if (opt.trace) write_file(opt.trace, log);
    if (opt.hash_frame) write_file(opt.hash_frame, gbfmt::state_line(m));

    std::printf(
        "timer run: simulated %llu T-cycles, %ld tima overflows, "
        "%ld interrupts serviced\n",
        static_cast<unsigned long long>(m.cyc), overflows, irqs);

    if (m.cpu.trap) {
        std::fprintf(stderr, "trap: unimplemented opcode at $%04X\n", m.cpu.pc);
        return 2;
    }
    return 0;
}
