#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "../shared/fixture.hpp"
#include "../shared/fnv.hpp"
#include "../shared/goldens.hpp"
#include "../shared/psx_mini.hpp"

// Built-in accuracy checks for ch50.
//
// A "built-in check" is a self-contained headless verification of one
// psx-mini subsystem against a golden pin from shared/goldens.hpp:
//
//   builtin.cpu_trace — run the fixture program twice; both traces must be
//                       byte-identical AND match the committed golden hash
//                       (the essence of golden trace testing).
//   builtin.vram      — draw a fixed scene, hash VRAM, compare.
//   builtin.spu       — render a fixed voice segment, hash PCM bytes.
//   builtin.dma       — block transfer; pin end-of-block register state.
//   builtin.gte       — MAC pipeline; pin ir[] outputs.
//   builtin.timer     — prescaled ticking; pin counter + target hits.
//   builtin.cdrom     — MSF/BCD sector walk; pin state hash.
//
// Every check is deterministic and allocation-free enough to run anywhere,
// which is what lets the same suite pass identically on a laptop and in CI.
namespace psxsuite {

using CheckFn = bool (*)(std::string* reason);

struct BuiltinCheck {
    const char* name;   // bare name; callers may prefix "builtin."
    const char* what;   // one-line description for --list
    CheckFn fn;
};

inline bool check_cpu_trace(std::string* why) {
    psxmini::Ram ram;
    for (unsigned i = 0; i < psxmini::kCpuProgram.size(); ++i)
        ram.store(4 * i, psxmini::kCpuProgram[i]);

    psxmini::Cpu a;
    const std::string t1 = psxmini::run_trace(a, ram, 64);

    psxmini::Cpu b;
    const std::string t2 = psxmini::run_trace(b, ram, 64);

    if (t1 != t2) {
        if (why) *why = "two runs of the fixture diverged (non-deterministic?)";
        return false;
    }
    const uint64_t h = psxmini::fnv64(t1);
    if (h != psxmini::kGoldenCpuTraceFnv64) {
        if (why) *why = "trace hash mismatch";
        return false;
    }
    if (a.r[1] != psxmini::kGoldenCpuFinalR1 || !a.halted ||
        ram.load(0x40) != psxmini::kGoldenCpuFinalR1) {
        if (why) *why = "final architectural state mismatch";
        return false;
    }
    return true;
}

inline bool check_vram_hash(std::string* why) {
    psxmini::Vram v;
    psxmini::gpu_fill(v, 0, 0, psxmini::Vram::kW, 8, 0x001F);          // sky
    psxmini::gpu_checker(v, 0, 8, psxmini::Vram::kW, 16, 0x03E0, 0x7C00);
    psxmini::gpu_fill(v, 28, 24, 8, 8, 0x7FFF);                        // block
    for (unsigned y = 24; y < 32; ++y) psxmini::gpu_hgradient(v, y, 0x0000, 0x0421);
    const uint64_t h = psxmini::hash_vram(v);
    if (h != psxmini::kGoldenVramFnv64) {
        if (why) *why = "vram hash mismatch";
        return false;
    }
    return true;
}

inline bool check_spu_hash(std::string* why) {
    psxmini::SpuVoice v;
    v.pitch = 0x0C00;
    const uint64_t h = psxmini::render_spu_hash(v, 320);
    if (h != psxmini::kGoldenSpuFnv64) {
        if (why) *why = "spu sample hash mismatch";
        return false;
    }
    return true;
}

inline bool check_dma_state(std::string* why) {
    std::array<uint32_t, 1024> ram{};
    for (unsigned i = 0; i < ram.size(); ++i)
        ram[i] = 0x1000u + i;  // stable ramp so dev contents are pinnable

    psxmini::DmaChan ch;
    ch.madr = 0x40;
    ch.bcr = 8u << 16;
    ch.step = 4;
    ch.enable = true;

    uint32_t dev[8];
    psxmini::dma_run_block(ch, ram, dev);

    const uint64_t h =
        psxmini::fnv64({reinterpret_cast<const uint8_t*>(dev), sizeof dev});
    if (ch.madr != psxmini::kGoldenDmaMadr || ch.bcr != 0 || ch.enable ||
        !ch.irq || h != psxmini::kGoldenDmaDevFnv64) {
        if (why) *why = "end-of-block dma state mismatch";
        return false;
    }
    return true;
}

inline bool check_gte_state(std::string* why) {
    psxmini::MiniGte g;
    g.m[0][0] = 4096;  g.m[0][1] = -8192; g.m[0][2] = 2048;
    g.m[1][0] = 2048;  g.m[1][1] = 4096;  g.m[1][2] = -1024;
    g.m[2][0] = 1024;  g.m[2][1] = 1024;  g.m[2][2] = 4096;
    g.tr[0] = 100; g.tr[1] = -50; g.tr[2] = 25;

    const int16_t v[3] = {4096, -2048, 1024};
    int32_t mac[3];
    int16_t ir[3];
    g.mul_vector(v, mac, ir);

    for (int i = 0; i < 3; ++i) {
        if (ir[i] != psxmini::kGoldenGteIr[i]) {
            if (why) *why = "gte ir[] mismatch";
            return false;
        }
    }
    return true;
}

inline bool check_timer_state(std::string* why) {
    psxmini::MiniTimer t;
    for (unsigned i = 0; i < 400; ++i) t.tick_sysclk();
    if (t.cnt != psxmini::kGoldenTimerCnt ||
        t.reached != psxmini::kGoldenTimerReached) {
        if (why) *why = "timer counter/target-hit state mismatch";
        return false;
    }
    return true;
}

inline bool check_cdrom_state(std::string* why) {
    // Address decode first: LBA 90123 -> 20:01:48 MSF, BCD-coded.
    const psxmini::Msf msf = psxmini::lba_to_msf(90123);
    if (psxmini::bcd_dec(msf.m) != 20 || psxmini::bcd_dec(msf.s) != 1 ||
        psxmini::bcd_dec(msf.f) != 48) {
        if (why) *why = "MSF address decode mismatch";
        return false;
    }
    psxmini::CdromState st;
    st.lba = 2;
    psxmini::cdrom_read_sectors(st, psxmini::kGoldenCdromCount);
    if (st.lba != psxmini::kGoldenCdromLba ||
        st.sectors_read != psxmini::kGoldenCdromCount ||
        st.data_hash != psxmini::kGoldenCdromHashFnv64) {
        if (why) *why = "sector-walk state mismatch";
        return false;
    }
    return true;
}

inline constexpr BuiltinCheck kBuiltinChecks[] = {
    {"cpu_trace", "CPU pc/op/cyc golden-trace compare over kCpuProgram",
     &check_cpu_trace},
    {"vram", "MiniGpu scene drawn into 64x32 VRAM, whole-surface hash",
     &check_vram_hash},
    {"spu", "MiniSpu voice rendered 320 samples, PCM byte hash",
     &check_spu_hash},
    {"dma", "DMA block transfer end-of-state pin (madr/bcr/enable/irq)",
     &check_dma_state},
    {"gte", "GTE MAC pipeline ir[] pin", &check_gte_state},
    {"timer", "timer prescaler + exact target-reload pin", &check_timer_state},
    {"cdrom", "CDROM MSF/BCD decode + sector-walk state pin",
     &check_cdrom_state},
};

inline constexpr size_t kBuiltinCheckCount =
    sizeof(kBuiltinChecks) / sizeof(kBuiltinChecks[0]);

// Accepts "builtin.cpu_trace" or the bare "cpu_trace".
inline const BuiltinCheck* find_builtin(const std::string& dotted) {
    std::string name = dotted;
    if (name.rfind("builtin.", 0) == 0) name = name.substr(8);
    for (const auto& c : kBuiltinChecks)
        if (name == c.name) return &c;
    return nullptr;
}

}  // namespace psxsuite
