#include <string>
int main(int argc, char** argv) {
    if (argc == 2 &&
        (std::string(argv[1]) == "--help" ||
         std::string(argv[1]) == "-h")) {
        std::fprintf(stderr,
                     "playable_nes — compose the course NES machine (see README.md)\n");
        return 0;
    }
    std::fprintf(stderr,
                 "TODO: compose the NES machine (ch19 CPU + ch20 NROM + "
                 "ch22 renderer + ch24 APU) and implement the gate CLI.\n"
                 "See tools/labs/nes_gate/nes_gate_runner.cpp for the "
                 "reference composition.\n");
    return 2;
}
