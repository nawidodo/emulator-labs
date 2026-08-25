// Headless Space Invaders machine runner — canonical CLI shape
// (docs/AUTHORING.md). Grading drives this binary directly.
//
//   si_runner --rom prog.bin [--cycles N | --frames N]
//             [--trace FILE] [--hash-frame FILE] [--dump-state FILE]
//             [--input-file FILE] [--headless] [--cpf N]
//
// --frames N runs N * kCyclesPerFrame T-states (N frame periods).
// --hash-frame FILE receives the FINAL frame as raw RGBA8888
// (224*256*4 bytes); its FNV-64 digest is the golden frame hash.
// --input-file feeds the input-latch protocol: one line per frame period,
// three hex bytes "P0 P1 P2" ('#' comments, blank lines skipped).
// --cpf overrides cycles-per-frame for small-scale determinism tests;
// production cadence uses the documented 32000.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "machine.hpp"

namespace {

struct Options {
    std::string rom;
    std::string trace;
    std::string hash_frame;
    std::string dump_state;
    std::string input_file;
    uint64_t max_cycles = 0;
    bool has_cycles = false;
    bool has_frames = false;
    uint64_t frames = 0;
    uint64_t cpf = si::kCyclesPerFrame;
};

bool read_file_bytes(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->assign((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
    return true;
}

std::vector<si::InputFrame> parse_input_script(const std::string& text) {
    std::vector<si::InputFrame> frames;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream ls(line);
        si::InputFrame f;
        unsigned v = 0;
        if (!(ls >> std::hex >> v)) continue;   // blank/comment-only line
        f.port0 = uint8_t(v);
        if (ls >> std::hex >> v) f.port1 = uint8_t(v);
        if (ls >> std::hex >> v) f.port2 = uint8_t(v);
        frames.push_back(f);
    }
    return frames;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
        std::cout <<
            "usage: si_runner --rom PATH [--cycles N | --frames N]\n"
            "                 [--trace FILE] [--hash-frame FILE]\n"
            "                 [--dump-state FILE] [--input-file FILE]\n"
            "                 [--cpf N] [--headless]\n";
        return 0;
    }

    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string {
            return (i + 1 < argc) ? argv[++i] : "";
        };
        if (a == "--rom") opts.rom = next();
        else if (a == "--trace") opts.trace = next();
        else if (a == "--hash-frame") opts.hash_frame = next();
        else if (a == "--dump-state") opts.dump_state = next();
        else if (a == "--input-file") opts.input_file = next();
        else if (a == "--cycles") { opts.max_cycles = strtoull(next().c_str(), nullptr, 0); opts.has_cycles = true; }
        else if (a == "--frames") { opts.frames = strtoull(next().c_str(), nullptr, 0); opts.has_frames = true; }
        else if (a == "--cpf") opts.cpf = strtoull(next().c_str(), nullptr, 0);
        else if (a == "--headless") { /* default mode */ }
        else { /* unknown flags ignored for forward compatibility */ }
    }

    if (opts.rom.empty()) {
        std::cerr << "error: --rom is required\n";
        return 2;
    }
    if (opts.has_cycles && opts.has_frames) {
        std::cerr << "error: --cycles and --frames are mutually exclusive\n";
        return 2;
    }

    std::vector<uint8_t> image;
    if (!read_file_bytes(opts.rom, &image)) {
        std::cerr << "error: cannot open rom '" << opts.rom << "'\n";
        return 2;
    }

    si::SpaceInvadersMachine m;
    m.load_rom(image.data(), image.size());
    m.timers().configure(opts.cpf, si::kIrqOpcodeEven, si::kIrqOpcodeOdd);

    std::vector<si::InputFrame> script;
    if (!opts.input_file.empty()) {
        std::ifstream in(opts.input_file, std::ios::binary);
        if (!in) {
            std::cerr << "error: cannot open input file '" << opts.input_file
                      << "'\n";
            return 2;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        script = parse_input_script(buf.str());
    }
    if (!script.empty()) m.set_inputs(script[0]);

    const uint64_t budget = opts.has_frames
                                ? opts.frames * opts.cpf
                                : (opts.has_cycles ? opts.max_cycles : 32000);

    std::ofstream trace_out;
    if (!opts.trace.empty()) {
        trace_out.open(opts.trace, std::ios::binary);
        if (!trace_out) {
            std::cerr << "error: cannot write trace '" << opts.trace << "'\n";
            return 2;
        }
    }

    // Frame-period bookkeeping: inputs advance at each frame boundary.

    while (!m.cpu().halted && m.cpu().cycles < budget) {
        const si::IrqRaise irq = m.timers().poll(m.cpu().cycles);
        if (irq.raised) {
            m.cpu().interrupt(irq.opcode);
            if (!script.empty()) {
                const size_t idx = size_t(m.cpu().cycles / opts.cpf);
                m.set_inputs(script[idx < script.size() ? idx : script.size() - 1]);
            }
        }
        if (trace_out.is_open()) trace_out << m.trace_line();
        m.cpu().step();
    }
    m.render();

    if (!opts.hash_frame.empty()) {
        std::ofstream out(opts.hash_frame, std::ios::binary);
        if (!out) {
            std::cerr << "error: cannot write frame '" << opts.hash_frame
                      << "'\n";
            return 2;
        }
        out.write(reinterpret_cast<const char*>(m.frame().rgba),
                  std::streamsize(si::Frame::kBytes));
    }

    const std::string state = m.state_line();
    std::cout << state << "\n";
    if (!opts.dump_state.empty()) {
        std::ofstream out(opts.dump_state, std::ios::binary);
        out << state << "\n";
    }
    return 0;
}
