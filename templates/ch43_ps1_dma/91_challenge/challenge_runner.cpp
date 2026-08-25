// challenge_runner — feed a GPU list through linked-list DMA, hash VRAM.
//
//   --list FILE     raw LE u32 chain words (loaded at word 0)
//   --madr HEX      start address (default 0)
//   --vram-out FILE dump raw VRAM (64x32 RGB15 = 4096 bytes)
//   --hash-out FILE write "fnv64=XXXXXXXXXXXXXXXX\n"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "challenge.hpp"

namespace {

std::vector<uint32_t> load_words(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    const size_t n = bytes.size() / 4;
    std::vector<uint32_t> words(n);
    if (n) std::memcpy(words.data(), bytes.data(), n * 4);
    return words;
}

uint32_t parse_hex(const char* s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "usage: challenge_runner --list FILE [--madr HEX] "
                     "[--vram-out FILE] [--hash-out FILE]\n";
        return 0;
    }
    std::string list_path, vram_out, hash_out;
    uint32_t madr = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--list") list_path = need("--list");
        else if (a == "--madr") madr = parse_hex(need("--madr"));
        else if (a == "--vram-out") vram_out = need("--vram-out");
        else if (a == "--hash-out") hash_out = need("--hash-out");
        else { std::cerr << "unknown arg " << a << "\n"; return 2; }
    }

    try {
        ps1::Ram ram;
        const auto words = load_words(list_path);
        for (size_t i = 0; i < words.size(); ++i)
            ram.write(static_cast<uint32_t>(4 * i), words[i]);

        ps1chal::Vram vram;
        const auto res = ps1chal::run_list(ram, madr, vram);

        if (!vram_out.empty()) {
            std::ofstream out(vram_out, std::ios::binary);
            out.write(reinterpret_cast<const char*>(vram.px.data()),
                      static_cast<std::streamsize>(vram.px.size() * 2));
            if (!out) throw std::runtime_error("cannot write " + vram_out);
        }
        if (!hash_out.empty()) {
            std::ofstream out(hash_out);
            out << "fnv64=" << std::hex << std::uppercase << res.vram_fnv
                << "\n";
            if (!out) throw std::runtime_error("cannot write " + hash_out);
        }

        std::cout << "packets=" << res.walk.packets << " terminated="
                  << (res.walk.terminated ? "yes" : "no") << " fnv64="
                  << std::hex << std::uppercase << res.vram_fnv << "\n";
        return res.walk.cap_hit ? 3 : 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
