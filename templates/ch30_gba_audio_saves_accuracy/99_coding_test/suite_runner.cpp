// Headless deterministic suite runner (curriculum §52 milestone): executes
// named subsystem suites with fixed inputs and prints pass/fail + digests.
//
//   ch30_suite_runner --list-suites
//   ch30_suite_runner --suite audio [--suite saves ...] [--isolate audio]
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstddef>
#define LABSTEST_NO_MAIN_GUARD
#include "../01_psg_mix/psg.hpp"
#include "../02_direct_sound/dsound.hpp"
#include "../03_save_flash/save.hpp"
#include "../04_eeprom_dmac/eeprom.hpp"

using namespace gba;

namespace {

struct SuiteResult {
    const char* name;
    bool ok;
    u64 digest;
};

SuiteResult run_audio() {
    SoundFifo a, b;
    a.reset();
    b.reset();
    for (int i = 0; i < kFifoSize; ++i) {
        a.push(u8(i * 5));
        b.push(u8(255 - i * 3));
    }
    std::vector<u16> pcm;
    u64 h = render_pcm(a, b, 6, 2, 1, 512, 64, 96, pcm);
    // Sanity anchors computed from the model itself.
    bool ok = pcm.size() == 96u && h != 0;
    return {"audio", ok, h};
}

SuiteResult run_saves() {
    FlashChip f(false);
    auto cmd = [&f](u8 op) {
        f.write(0x0E000000, 0xAA);
        f.write(0x0E000002, 0x55);
        f.write(0x0E000000, op);
    };
    cmd(0xA0);
    f.write(0x100, 0x3C);
    cmd(0x90);
    bool id_ok = f.read(0x0E000001) == kFlashDevId64K;
    cmd(0xF0);
    bool prog_ok = f.read(0x100) == 0x3C;
    cmd(0x80);
    cmd(0x10);  // chip erase
    bool erased = f.read(0x100) == 0xFF;

    Sram sram;
    sram.write(0x7FFF, 0x42);
    bool sram_ok = sram.read(0x7FFF) == 0x42;

    u64 digest = u64(id_ok) | u64(prog_ok) << 1 | u64(erased) << 2 |
                 u64(sram_ok) << 3 | (u64(kFlashMfgId) << 32);
    return {"saves", id_ok && prog_ok && erased && sram_ok, digest};
}

SuiteResult run_eeprom() {
    Eeprom e(kEeprom512B);
    std::vector<int> bits = {1, 0, 0};
    u32 addr = 9;
    for (int i = 5; i >= 0; --i) bits.push_back((addr >> i) & 1);
    u64 data = 0x0F0FCCCC3333AAAAull;
    for (int i = 63; i >= 0; --i) bits.push_back((int(data >> i) & 1));
    bits.push_back(0);
    while (bits.size() % 16) bits.push_back(0);
    std::vector<u16> words;
    for (size_t i = 0; i < bits.size(); i += 16) {
        u16 w = 0;
        for (int b = 0; b < 16; ++b) w = u16(w | bits[i + b] << (15 - b));
        words.push_back(w);
    }
    e.feed_dma_stream(words.data(), int(words.size()));
    e.stop();
    bool wrote = false;
    for (int off = 72; off < 80; ++off) wrote |= e.mem[size_t(off)] != 0xFF;
    return {"eeprom", wrote, u64(wrote) * data ^ u64(e.addr_width())};
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::printf(
            "usage: ch30_suite_runner [options]\n"
            "  --list-suites       print available suite names\n"
            "  --suite NAME        run one suite (audio, saves, eeprom)\n"
            "  --isolate NAME      run everything EXCEPT NAME\n");
        return 0;
    }

    std::vector<std::string> wanted;
    std::string isolate;
    bool list = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list-suites") list = true;
        else if (arg == "--suite" && i + 1 < argc) wanted.push_back(argv[++i]);
        else if (arg == "--isolate" && i + 1 < argc) isolate = argv[++i];
    }
    if (list || (wanted.empty() && isolate.empty())) {
        std::printf("suites: audio saves eeprom\n");
        return list ? 0 : 0;
    }

    std::vector<SuiteResult> results;
    auto should_run = [&](const char* name) {
        if (!isolate.empty() && isolate == name) return false;
        if (wanted.empty()) return true;
        for (const auto& w : wanted)
            if (w == name) return true;
        return false;
    };
    if (should_run("audio")) results.push_back(run_audio());
    if (should_run("saves")) results.push_back(run_saves());
    if (should_run("eeprom")) results.push_back(run_eeprom());

    int failed = 0;
    for (const auto& r : results) {
        std::printf("[%4s] suite=%-7s digest=%016llX\n", r.ok ? "PASS" : "FAIL",
                    r.name, (unsigned long long)r.digest);
        if (!r.ok) ++failed;
    }
    return failed == 0 ? 0 : 1;
}
