// ch01/99_coding_test — machine-checked gate for the chapter coding test.
//
// Modes (argv[1]):
//   coding-test   run the STUDENT generator (../03_generator_starter/
//                 generate_skel.py) over the unseen seven-level template
//                 for every valid skeleton version (--todo 1..7, no-todo,
//                 --mode solution) and FNV-1a-64-compare every output
//                 file, manifest included, against recorded goldens.
//   determinism   run one variant twice and require byte-identical trees.
//   challenge     run the student generator in solution mode over the
//                 three sentinel-encoded challenge fixtures and compare
//                 against goldens.
//
// Exit 0 iff the selected mode passes. Goldens were produced by running
// the reference solution twice (see tests/public/ch01_lab_infrastructure/
// provenance.md).

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "goldens.hpp"

namespace fs = std::filesystem;

namespace {

#ifndef EXERCISE_DIR
#define EXERCISE_DIR "."
#endif

std::vector<uint8_t> read_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
}

uint64_t fnv1a64(const std::vector<uint8_t>& data) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t b : data) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

std::string to_hex16(uint64_t v) {
    static const char kDigits[] = "0123456789ABCDEF";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) {
        s[i] = kDigits[v & 0xF];
        v >>= 4;
    }
    return s;
}

struct Scratch {
    fs::path path;
    explicit Scratch(unsigned seq)
        : path(fs::temp_directory_path() /
               ("labs_ch01_99_" + std::to_string(seq))) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

bool run_student(const std::string& args_line, const fs::path& out_dir,
                 const std::string& template_dir) {
    std::string cmd =
        "python3 \"" + std::string(EXERCISE_DIR) +
        "/../03_generator_starter/generate_skel.py\" " + args_line +
        " --template-dir \"" + template_dir + "\" --out \"" +
        out_dir.string() + "\"";
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::cout << "  generator failed (rc=" << rc << "): " << cmd << "\n";
        return false;
    }
    return true;
}

// Compare the generated tree against one golden key group.
bool compare_tree(const fs::path& out_dir, const char* key) {
    std::vector<const ch01_goldens::Entry*> expected;
    for (const auto& e : ch01_goldens::kEntries) {
        if (std::strcmp(e.key, key) == 0) {
            expected.push_back(&e);
        }
    }
    if (expected.empty()) {
        std::cout << "  no goldens recorded for '" << key << "'\n";
        return false;
    }

    bool ok = true;
    for (const auto* e : expected) {
        const fs::path p = out_dir / e->path;
        if (!fs::is_regular_file(p)) {
            std::cout << "  missing output: " << e->path << "\n";
            ok = false;
            continue;
        }
        const std::string got = to_hex16(fnv1a64(read_bytes(p)));
        if (got != e->fnv) {
            std::cout << "  hash mismatch: " << e->path << " got " << got
                      << " want " << e->fnv << "\n";
            ok = false;
        }
    }
    for (const auto& entry : fs::recursive_directory_iterator(out_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string rel =
            entry.path().generic_string().substr(out_dir.string().size() + 1);
        bool known = false;
        for (const auto* e : expected) {
            if (rel == e->path) {
                known = true;
                break;
            }
        }
        if (!known) {
            std::cout << "  unexpected extra file: " << rel << "\n";
            ok = false;
        }
    }
    return ok;
}

int mode_coding_test() {
    unsigned failures = 0;
    unsigned runs = 0;
    const std::string tpl = std::string(EXERCISE_DIR) + "/data/seven_level";
    for (const auto& variant :
         {"none", "1", "2", "3", "4", "5", "6", "7", "solution"}) {
        ++runs;
        Scratch scratch{runs};
        std::string args;
        if (std::string(variant) == "solution") {
            args = "--mode solution";
        } else if (std::string(variant) != "none") {
            args = std::string("--todo ") + variant;
        }
        const std::string key = "seven/" + std::string(variant);
        if (!run_student(args, scratch.path, tpl)) {
            ++failures;
            continue;
        }
        std::cout << "[" << key << "]\n";
        if (!compare_tree(scratch.path, key.c_str())) {
            ++failures;
        }
    }
    std::cout << "== coding-test: " << (runs - failures) << "/" << runs
              << " variants match\n";
    return failures == 0 ? 0 : 1;
}

int mode_determinism() {
    Scratch a{101}, b{102};
    const std::string tpl = std::string(EXERCISE_DIR) + "/data/seven_level";
    if (!run_student("--todo 5", a.path, tpl) ||
        !run_student("--todo 5", b.path, tpl)) {
        return 1;
    }
    for (const auto& e : ch01_goldens::kEntries) {
        if (std::strncmp(e.key, "seven/5", 7) != 0) {
            continue;
        }
        if (read_bytes(a.path / e.path) != read_bytes(b.path / e.path)) {
            std::cout << "  nondeterministic bytes: " << e.path << "\n";
            return 1;
        }
    }
    std::cout << "== determinism: two identical runs\n";
    return 0;
}

int mode_challenge() {
    unsigned failures = 0;
    const char* minis[] = {"challenge_a", "challenge_b", "challenge_c"};
    unsigned run = 200;
    for (const char* mini : minis) {
        Scratch scratch{run += 10};
        const std::string tpl = std::string(EXERCISE_DIR) + "/data/" + mini;
        if (!run_student("--mode solution", scratch.path, tpl)) {
            ++failures;
            continue;
        }
        const std::string key = std::string(mini) + "/solution";
        std::cout << "[" << key << "]\n";
        if (!compare_tree(scratch.path, key.c_str())) {
            ++failures;
        }
    }
    std::cout << "== challenge: " << (3 - failures) << "/3 fixtures match\n";
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "coding-test") {
        return mode_coding_test();
    }
    if (mode == "determinism") {
        return mode_determinism();
    }
    if (mode == "challenge") {
        return mode_challenge();
    }
    std::cerr << "usage: ch01_99_coding_test "
                 "{coding-test|determinism|challenge}\n";
    return 2;
}
