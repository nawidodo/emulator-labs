// Conformance tests for the suite-runner contract: filtering changes which
// suites execute, never their results.
#define LABSTEST_MAIN
#include <string>
#include <vector>
#include "labstest.hpp"
#include "../01_psg_mix/psg.hpp"
#include "../02_direct_sound/dsound.hpp"
#include "../03_save_flash/save.hpp"
#include "../04_eeprom_dmac/eeprom.hpp"
#include <cstddef>

using namespace gba;

struct SuiteResult {
    const char* name;
    bool ok;
    u64 digest;
};

namespace {

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
    return {"audio", pcm.size() == 96u && h != 0, h};
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
    cmd(0x10);
    bool erased = f.read(0x100) == 0xFF;
    return {"saves", id_ok && prog_ok && erased,
            u64(prog_ok) | (u64(erased) << 8)};
}

SuiteResult run_eeprom() {
    Eeprom e(kEeprom512B);
    std::vector<int> bits = {1, 0, 0};
    for (int i = 5; i >= 0; --i) bits.push_back((9 >> i) & 1);
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
    int wrote_any = 0;
    for (int off = 72; off < 80; ++off)
        wrote_any |= e.mem[size_t(off)] != 0xFF ? 1 : 0;
    return {"eeprom", wrote_any != 0, wrote_any * 0xFFull};
}

}  // namespace

TEST(suites, each_suite_passes_with_stable_digest) {
    SuiteResult a = run_audio();
    SuiteResult s = run_saves();
    SuiteResult e = run_eeprom();
    EXPECT_TRUE(a.ok);
    EXPECT_TRUE(s.ok);
    EXPECT_TRUE(e.ok);

    // Determinism: re-running yields identical digests.
    EXPECT_EQ(run_audio().digest, a.digest);
    EXPECT_EQ(run_saves().digest, s.digest);
    EXPECT_EQ(run_eeprom().digest, e.digest);
}
