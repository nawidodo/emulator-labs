#pragma once
// System wiring for the ch49 stand-in PS1 SoC: mini core + GPU/DMA/CD/
// SPU/INTC behind ONE event scheduler.
//
// Execution model (the reference architecture):
//
//   CPU executes -> scheduler time advances -> due events dispatch ->
//   devices update -> interrupt state changes -> CPU continues
//
// The CPU is itself just events: one instruction per "cpu" event at a
// fixed kInstrCycles spacing. When the DMA stand-in is draining, the next
// CPU event is scheduled at the drain deadline instead — the bus stall,
// expressed structurally rather than as a busy-wait. Nothing in this file
// ever polls: every device advances only when an event fires.
//
// Templated on the scheduler type so 90_debug can swap in its
#include <array>
#include <sstream>
// instrumented scheduler without touching this wiring.
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

#include "../01_scheduler_core/scheduler.hpp"
#include "core.hpp"
#include "devices.hpp"

namespace ps1sys {

constexpr uint32_t kRamWords = 16384;  // 64 KiB stand-in main RAM

template <class SchedT>
class System : public Bus {
public:
    explicit System(SchedT& s) : sched_(s) {
        intc.bind(&log_);
        gpu.bind(&log_, &intc);
        dma.bind(&log_, &intc);
        cd.bind(&log_, &intc);
        spu.bind(&log_, &intc);
    }

    void set_cpu_enabled(bool on) { cpu_enabled_ = on; }

    void reset() {
        sched_.clear();
        core.reset();
        ram_.fill(0);
        intc.reset();
        gpu.reset();
        dma.reset();
        cd.reset();
        spu.reset();
        log_.clear();
        trace_.clear();
        halted_ = false;
        bcr_latch_ = 0;
        log_event(log_, 0, "boot");
        // Two roots drive everything else: the instruction stream and the
        // SPU sample chain (44100 Hz exactly = every 768 master cycles).
        if (cpu_enabled_) schedule_cpu(0);
        schedule_spu_tick(kSpuSampleCycles);
    }

    // Boot image: raw little-endian words copied into RAM starting at
    // word 0; execution entry is word 0 (pc=0).
    void load_rom(const uint8_t* bytes, size_t n) {
        for (size_t i = 0; i + 4 <= n && i / 4 < kRamWords; i += 4)
            ram_[i / 4] = uint32_t(bytes[i]) | uint32_t(bytes[i + 1]) << 8 |
                          uint32_t(bytes[i + 2]) << 16 |
                          uint32_t(bytes[i + 3]) << 24;
    }

    // Deterministic device script applied at cycle 0 right after boot
    // scheduling (see 03_boot_runner/SPEC.md for the grammar).
    bool apply_script(std::istream& in) {
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line[0] == '#') continue;
            std::istringstream ss(line);
            std::string cmd;
            if (!(ss >> cmd) || cmd.empty()) continue;
            if (cmd == "MASK") {
                unsigned m = 0;
                if (!(ss >> std::hex >> m)) return false;
                intc.set_mask(m);
            } else if (cmd == "DMA") {
                unsigned w = 0;
                if (!(ss >> std::dec >> w)) return false;
                bcr_latch_ = w;
                store32(kDmaChcr, 1);
            } else if (cmd == "GPUCMD") {
                unsigned g = 0;
                if (!(ss >> std::hex >> g)) return false;
                store32(kGpuGp0, g);
            } else if (cmd == "CDREAD") {
                store32(kCdCmd, 1);
            } else if (cmd == "SPUON") {
                store32(kSpuCtrl, 1);
            } else {
                return false;
            }
        }
        return true;
    }

    void run_until(uint64_t cycles) { sched_.run_until(cycles); }

    // --- Bus (used by the core AND available for direct MMIO pokes) ----
    uint32_t load32(uint32_t addr) override {
        switch (addr) {
            case kIStat: return intc.status();
            case kGpuGp0: return gpu.stat();
            default: return ram_[(addr >> 2) & (kRamWords - 1)];
        }
    }

    void store32(uint32_t addr, uint32_t val) override {
        const uint64_t now = sched_.now();
        switch (addr) {
            case kIStat: intc.ack(val); return;
            case kIMask: intc.set_mask(val); return;
            case kDmaBcr: bcr_latch_ = val; return;
            case kDmaChcr:
                if ((val & 1u) != 0) dma.start(sched_, now, bcr_latch_);
                return;
            case kCdCmd:
                if (val != 0) cd.read_sector(sched_, now);
                return;
            case kGpuGp0: gpu.gp0(sched_, now, val); return;
            case kGpuGp1: gpu.gp1(val); return;
            case kSpuCtrl: spu.set_ctrl(val); return;
            case kMilestone:
                log_event(log_, now, "milestone", "val=%u", val);
                return;
            default:
                ram_[(addr >> 2) & (kRamWords - 1)] = val;
                return;
        }
    }

    // --- Observability --------------------------------------------------
    const Log& event_log() const { return log_; }
    const std::vector<std::string>& trace() const { return trace_; }
    bool halted() const { return halted_; }
    uint64_t now() const { return sched_.now(); }
    SchedT& sched() { return sched_; }
    Intc& intc_ctrl() { return intc; }
    Gpu& gpu_ctrl() { return gpu; }
    Dma& dma_ctrl() { return dma; }
    Cd& cd_ctrl() { return cd; }
    Spu& spu_ctrl() { return spu; }

private:
    void schedule_cpu(uint64_t ts) {
        sched_.schedule(ts, [this] { cpu_event(); }, "cpu");
    }

    void schedule_spu_tick(uint64_t ts) {
        // Recurrence anchored on the PREVIOUS deadline (never on "now"),
        // so sample boundaries stay exactly 768 cycles apart forever even
        // if a future feature delays dispatch.
        sched_.schedule(ts, [this, ts] {
            spu.on_sample_tick(ts);
            schedule_spu_tick(ts + kSpuSampleCycles);
        }, "spu_sample");
    }

    void cpu_event() {
        if (halted_) return;
        if (dma.busy()) {  // bus stall: hold the CPU until the drain ends
            schedule_cpu(dma.done_time());
            return;
        }
        const uint32_t pc_before = core.pc;
        const uint32_t w = core.step(*this);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "pc=%08X op=%08X cyc=%llu",
                      pc_before * 4u, w,
                      static_cast<unsigned long long>(sched_.now()));
        trace_.emplace_back(buf);
        if (core.halted) {
            halted_ = true;
            log_event(log_, sched_.now(), "halt");
            return;
        }
        // The instruction we just ran may have kicked a DMA transfer; the
        // stall check must see it, hence the second test below.
        if (dma.busy()) schedule_cpu(dma.done_time());
        else schedule_cpu(sched_.now() + kInstrCycles);
    }

    SchedT& sched_;
    Core core;
    Intc intc;
    Gpu gpu;
    Dma dma;
    Cd cd;
    Spu spu;
    Log log_;
    std::vector<std::string> trace_;
    std::array<uint32_t, kRamWords> ram_{};
    bool cpu_enabled_ = true;
    bool halted_ = false;
    uint32_t bcr_latch_ = 0;
};

}  // namespace ps1sys
