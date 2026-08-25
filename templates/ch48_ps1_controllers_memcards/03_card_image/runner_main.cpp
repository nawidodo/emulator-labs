// Headless SIO runner (curriculum §52 CLI shape).
//
//   ch48_03_sio_runner [--rom card.mcr] --input-file script.txt \
//       --hash-frame out.bin --trace events.log --headless \
//       [--cycles N] [--frames N]
//
// The optional ROM argument is a raw 131072-byte .mcr card image mounted
// into SLOT 1's memory card before the script runs.
//
// The input file is a deterministic transaction script, one command per
// line; blank lines and `#` comments are ignored:
//
//   SLOT <n>            select the active slot (n = 0 or 1); writes CTRL
//                       bits 12/13 accordingly
//   PAD <word-hex>      load an ACTIVE-LOW button halfword into the active
//                       slot's pad (e.g. PAD F7BF presses cross+start)
//   XFER <b0> [b1 ...]  push hex bytes through 1F801040; every response
//                       byte is appended to the hash-frame dump
//   CARDFILE <path>     mount a .mcr image into the active slot's card
//                       (directory bad-block flags are rescanned)
//   CARDSTORE <path>    write the active slot's card image back to a file
//   RUN                 no-op commit marker (kept for readability)
//
// Output: all XFER response bytes concatenated into the --hash-frame file;
// its FNV-1a 64 digest (uppercase hex) is printed to stdout as `fnv64 XXXX`.
//
// Trace lines follow the canonical key=value shape with cyc=<n>; one line
// per transferred byte, cyc counts serial bits (bytes * 8):
//
//   pc=1f801040 op=<tx> reg=rx=<rx> reg=slot=<n> cyc=<bits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "card_image.hpp"
#include "sio_bus.hpp"
#include "../shared/fnv.hpp"

namespace {

bool load_card_file(sio::MemCard& card, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char buf[sio::kImageBytes];
    in.read(buf, sio::kImageBytes);
    if (in.gcount() != static_cast<std::streamsize>(sio::kImageBytes)) {
        return false;
    }
    sio::CardImage img;
    img.bytes() = *reinterpret_cast<
        const std::array<uint8_t, sio::CardImage::kSize>*>(buf);
    img.mount_into(card);
    return true;
}

bool store_card_file(const sio::MemCard& card, const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    std::array<uint8_t, sio::kImageBytes> img{};
    card.export_image(img.data());
    out.write(reinterpret_cast<const char*>(img.data()),
              static_cast<std::streamsize>(img.size()));
    return true;
}

uint8_t parse_byte(const std::string& tok) {
    return static_cast<uint8_t>(std::stoul(tok, nullptr, 16));
}

void run_script(sio::SioBus& bus, const std::string& path,
                std::vector<uint8_t>& responses,
                std::vector<std::string>& trace) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "sio_runner: cannot open script " << path << "\n";
        std::exit(2);
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd[0] == '#') continue;

        if (cmd == "SLOT") {
            int n = 0;
            ss >> n;
            uint16_t c = bus.ctrl();
            c &= ~(sio::CTRL_SLOT1 | sio::CTRL_SLOT2);
            c |= (n == 1) ? sio::CTRL_SLOT2 : sio::CTRL_SLOT1;
            bus.write_ctrl(c);
        } else if (cmd == "PAD") {
            std::string w;
            ss >> w;
            const uint16_t word = static_cast<uint16_t>(
                std::stoul(w, nullptr, 16));
            const int slot = bus.active_slot();
            if (slot >= 0) bus.pads[slot].set_buttons(
                sio::buttons_from_report(word));
        } else if (cmd == "XFER") {
            std::string tok;
            const int slot = bus.active_slot();
            while (ss >> tok) {
                const uint8_t tx = parse_byte(tok);
                const uint8_t rx = bus.xfer(tx);
                responses.push_back(rx);
                std::ostringstream ts;
                ts << "pc=1f801040 op=" << std::hex << static_cast<int>(tx)
                   << " reg=rx=" << static_cast<int>(rx)
                   << " reg=slot=" << std::dec << slot
                   << " cyc=" << bus.serial_bits();
                trace.push_back(ts.str());
            }
        } else if (cmd == "CARDFILE") {
            std::string p;
            ss >> p;
            const int slot = bus.active_slot();
            if (slot >= 0 && !load_card_file(bus.cards[slot], p)) {
                std::cerr << "sio_runner: cannot mount card " << p << "\n";
                std::exit(2);
            }
        } else if (cmd == "CARDSTORE") {
            std::string p;
            ss >> p;
            const int slot = bus.active_slot();
            if (slot >= 0 && !store_card_file(bus.cards[slot], p)) {
                std::cerr << "sio_runner: cannot store card " << p << "\n";
                std::exit(2);
            }
        } else if (cmd == "RUN") {
            // no-op commit marker
        } else {
            std::cerr << "sio_runner: unknown command '" << cmd << "'\n";
            std::exit(2);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, input_path, hash_path, trace_path;
    bool headless = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "sio_runner: missing value for " << what << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--rom") rom_path = next("--rom");
        else if (a == "--input-file") input_path = next("--input-file");
        else if (a == "--hash-frame") hash_path = next("--hash-frame");
        else if (a == "--trace") trace_path = next("--trace");
        else if (a == "--cycles" || a == "--frames") next(a.c_str());  // ignored
        else if (a == "--headless") headless = true;
        else if (a == "--help") {
            std::cout <<
                "usage: ch48_03_sio_runner [--rom card.mcr] --input-file "
                "script.txt --hash-frame out.bin [--trace log] --headless\n";
            return 0;
        } else {
            std::cerr << "sio_runner: unknown flag " << a << "\n";
            return 2;
        }
    }

    sio::SioBus bus;
    bus.write_ctrl(sio::CTRL_SLOT1);  // default: slot 1 active

    if (!rom_path.empty()) {
        if (!load_card_file(bus.cards[0], rom_path)) {
            std::cerr << "sio_runner: bad or missing ROM " << rom_path << "\n";
            return 2;
        }
    }

    std::vector<uint8_t> responses;
    std::vector<std::string> trace;
    if (!input_path.empty()) run_script(bus, input_path, responses, trace);

    if (!trace_path.empty()) {
        std::ofstream tf(trace_path, std::ios::trunc);
        for (const auto& t : trace) tf << t << "\n";
    }

    uint64_t h = sio::fnv64(responses);
    if (!hash_path.empty()) {
        std::ofstream hf(hash_path, std::ios::binary | std::ios::trunc);
        hf.write(reinterpret_cast<const char*>(responses.data()),
                 static_cast<std::streamsize>(responses.size()));
    }

    (void)headless;
    char digest[32];
    std::snprintf(digest, sizeof(digest), "%016llX",
                  static_cast<unsigned long long>(h));
    std::cout << "fnv64 " << digest << "\n";
    std::cout << "bytes " << responses.size() << "\n";
    return 0;
}
