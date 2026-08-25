// Headless interrupt-cadence runner (canonical CLI shape, plus --cpf for
// test-scale timing):
//
//   i8080_cadence_runner --rom prog.bin [--cycles N | --frames N]
//                        [--trace FILE] [--dump-state FILE] [--cpf N]
//
// Default cadence is the documented board timing (32000 cycles per frame,
// RST 08 / RST 10 alternating); --cpf shrinks the frame period so small
// fixture programs can be observed over many frames.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "frame_machine.hpp"

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
        std::cout <<
            "usage: i8080_cadence_runner --rom PATH "
            "[--cycles N | --frames N]\n"
            "                           [--trace FILE] "
            "[--dump-state FILE]\n"
            "                           [--cpf N] [--headless]\n";
        return 0;
    }

    std::string rom_path, trace_path, dump_path;
    uint64_t max_cycles = 0, frames = 0, cpf = si::kCyclesPerFrame;
    bool has_cycles = false, has_frames = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : "";
        };
        if (a == "--rom") rom_path = next();
        else if (a == "--trace") trace_path = next();
        else if (a == "--dump-state") dump_path = next();
        else if (a == "--cycles") { max_cycles = strtoull(next().c_str(), nullptr, 0); has_cycles = true; }
        else if (a == "--frames") { frames = strtoull(next().c_str(), nullptr, 0); has_frames = true; }
        else if (a == "--cpf") cpf = strtoull(next().c_str(), nullptr, 0);
        else if (a == "--headless") {}
        else {}
    }

    if (rom_path.empty()) {
        std::cerr << "error: --rom is required\n";
        return 2;
    }
    if (has_cycles && has_frames) {
        std::cerr << "error: --cycles and --frames are mutually exclusive\n";
        return 2;
    }

    std::ifstream rom(rom_path, std::ios::binary);
    if (!rom) {
        std::cerr << "error: cannot open rom '" << rom_path << "'\n";
        return 2;
    }
    const std::vector<uint8_t> program{
        (std::istreambuf_iterator<char>(rom)),
        std::istreambuf_iterator<char>()};

    si::FrameMachine m;
    m.load(program);
    m.configure_timing(cpf, si::kIrqOpcodeEven, si::kIrqOpcodeOdd);

    const uint64_t budget =
        has_frames ? frames * cpf : (has_cycles ? max_cycles : cpf);

    std::ofstream trace_out;
    if (!trace_path.empty()) {
        trace_out.open(trace_path, std::ios::binary);
        if (!trace_out) {
            std::cerr << "error: cannot write trace '" << trace_path << "'\n";
            return 2;
        }
    }

    m.run(budget, trace_out.is_open() ? &trace_out : nullptr);

    char buf[160];
    const i8080::Cpu& c = m.cpu();
    const uint8_t f =
        i8080::pack_psw(c.s, c.z, c.ac, c.p, c.cy);
    std::snprintf(buf, sizeof buf,
                  "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X "
                  "SP=%04X PC=%04X cyc=%llu raises=%d accepts=%d",
                  c.a, f, c.b, c.c, c.d, c.e, c.h, c.l, c.sp, c.pc,
                  static_cast<unsigned long long>(c.cycles),
                  m.raises(), m.accepts());
    std::cout << buf << "\n";
    if (!dump_path.empty()) {
        std::ofstream out(dump_path, std::ios::binary);
        out << buf << "\n";
    }
    return 0;
}
