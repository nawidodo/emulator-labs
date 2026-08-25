// mdec_cli — DMA-fed MDEC stream decoder CLI (chapter 46).
//
//   --stream FILE   compressed macroblock stream
//   --out FILE      raw RGB15 output (512 bytes per macroblock)
//   --hash-out FILE writes "fnv64=XXXXXXXXXXXXXXXX\n"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "mdec_core.hpp"

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "usage: mdec_cli --stream FILE [--out FILE] "
                     "[--hash-out FILE]\n";
        return 0;
    }
    std::string stream, out, hash_out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--stream") stream = need("--stream");
        else if (a == "--out") out = need("--out");
        else if (a == "--hash-out") hash_out = need("--hash-out");
        else { std::cerr << "unknown arg " << a << "\n"; return 2; }
    }
    try {
        const auto res = mchal::decode_stream(stream);
        if (!out.empty()) {
            std::ofstream f(out, std::ios::binary);
            f.write(reinterpret_cast<const char*>(res.pixels.data()),
                    static_cast<std::streamsize>(res.pixels.size() * 2));
            if (!f) {
                std::cerr << "error: cannot write " << out << "\n";
                return 2;
            }
        }
        const uint64_t h =
            mchal::fnv1a64(res.pixels.data(), res.pixels.size() * 2);
        if (!hash_out.empty()) {
            std::ofstream f(hash_out);
            f << "fnv64=" << std::hex << std::uppercase << h << "\n";
            if (!f) {
                std::cerr << "error: cannot write " << hash_out << "\n";
                return 2;
            }
        }
        std::cout << "macroblocks=" << res.macroblocks << " fnv64="
                  << std::hex << std::uppercase << h << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
