#pragma once
// System glue: wires Bus + DMA channels + timers + IRQ controller into the
// guest-cycle scheduler. Provided code — the student tasks live in
// scheduler.hpp. Model types are duplicated from exercises 01-04 so this
// directory builds standalone.
#include <cstdio>
#include <string>
#include <vector>
#include "scheduler.hpp"

namespace gba {

struct Bus {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kEwramBase = 0x02000000;
    static constexpr u32 kIwramBase = 0x03000000;

    u8 ewram[0x40000] = {};
    u8 iwram[0x8000] = {};
    u16 io[0x100] = {};

    static u16 rd_le(const u8* p) { return u16(p[0]) | u16(p[1]) << 8; }
    static void wr_le(u8* p, u16 v) {
        p[0] = u8(v);
        p[1] = u8(v >> 8);
    }
    u16 rd16(u32 addr) const {
        switch ((addr >> 24) & 7) {
            case 2: return rd_le(ewram + ((addr - kEwramBase) & 0x3FFFE));
            case 3: return rd_le(iwram + ((addr - kIwramBase) & 0x7FFE));
            case 4: return io[((addr - kIoBase) >> 1) & 0xFF];
            default: return 0;
        }
    }
    void wr16(u32 addr, u16 v) {
        switch ((addr >> 24) & 7) {
            case 2: wr_le(ewram + ((addr - kEwramBase) & 0x3FFFE), v); break;
            case 3: wr_le(iwram + ((addr - kIwramBase) & 0x7FFE), v); break;
            case 4: io[((addr - kIoBase) >> 1) & 0xFF] = v; break;
            default: break;
        }
    }
};

struct DmaRegs {
    u32 sad = 0;
    u32 dad = 0;
    u16 count = 0;
    u16 control = 0;
    bool enable() const { return (control >> 15) & 1; }
    bool irq_en() const { return (control >> 14) & 1; }
    int timing() const { return (control >> 12) & 3; }
    bool word() const { return (control >> 10) & 1; }
    bool repeat() const { return (control >> 9) & 1; }
    int src_ctl() const { return (control >> 7) & 3; }
    int dst_ctl() const { return (control >> 5) & 3; }
    u32 effective_count() const { return count == 0 ? 0x10000u : u32(count); }
};

enum class Trigger { Immediate, VBlank, HBlank, Special };

inline s32 dma_addr_step(int ctl_bits, bool word) {
    s32 size = word ? 4 : 2;
    switch (ctl_bits & 3) {
        case 0:
        case 1: return size;
        case 2: return -size;
        default: return 0;
    }
}

inline u64 dma_run(Bus& bus, DmaRegs& r, bool& irq_raised) {
    irq_raised = false;
    if (!r.enable()) return 0;
    u64 cycles = 0;
    s32 ss = dma_addr_step(r.src_ctl(), r.word());
    s32 ds = dma_addr_step(r.dst_ctl(), r.word());
    u32 sad = r.sad, dad = r.dad;
    for (u32 i = 0; i < r.effective_count(); ++i) {
        bus.wr16(dad, bus.rd16(sad));
        sad += u32(ss);
        dad += u32(ds);
        cycles += 6;
    }
    (void)r.word();
    if (r.irq_en()) irq_raised = true;
    r.control &= ~0x8000u;  // triggered transfers re-arm via repeat handling
    if (r.repeat()) r.control |= 0x8000u;
    return cycles;
}

inline bool dma_fires_on(const DmaRegs& r, Trigger trig, bool already) {
    if (!r.enable()) return false;
    if (r.timing() == 0 || Trigger(r.timing()) != trig) return false;
    return r.repeat() || !already;
}

struct Timer {
    u16 counter = 0;
    u16 reload = 0;
    u16 control = 0;
    bool enable() const { return (control >> 7) & 1; }
    bool cascade() const { return (control >> 2) & 1; }
    bool irq_en() const { return (control >> 6) & 1; }
    int field() const { return control & 3; }
};

struct IrqController {
    u16 ie = 0, iff = 0;
    bool ime = false;
    void raise(u16 bits) { iff |= bits; }
};

constexpr u16 kIrqTimer0 = 1 << 3;
constexpr u16 kIrqDma0 = 1 << 8;

struct HWSystem {
    Bus bus;
    DmaRegs ch[4];
    bool fired[4] = {};
    Timer tm[4];
    IrqController irq;
    Scheduler sched;
    bool tm_armed[4] = {};  // one live overflow event per timer
    std::vector<std::string> trace;

    void log(const std::string& s) { trace.push_back(s); }

    // ---- video ----
    void schedule_video() {
        u64 cyc;
        VideoEvent kind;
        next_video_event(sched.now, &cyc, &kind);
        sched.schedule(cyc, [this, cyc, kind] {
            dispatch_video(kind, cyc);
            schedule_video();
        });
    }

    void dispatch_video(VideoEvent kind, u64 cyc) {
        if (kind == VideoEvent::LineStart) return;
        Trigger trig =
            kind == VideoEvent::HBlankStart ? Trigger::HBlank : Trigger::VBlank;
        int order[4] = {0, 1, 2, 3};
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j)
                if (order[j] < order[i]) {
                    int t = order[i];
                    order[i] = order[j];
                    order[j] = t;
                }
        for (int c : order) {
            if (!dma_fires_on(ch[c], trig, fired[c])) continue;
            fired[c] = true;
            bool irq_raised = false;
            u64 cost = dma_run(bus, ch[c], irq_raised);
            sched.begin_dma_burst(cyc, cost);
            char buf[80];
            std::snprintf(buf, sizeof(buf), "op=dma ch=%d cyc=%llu", c,
                          (unsigned long long)cyc);
            log(buf);
            if (irq_raised) {
                irq.raise(u16(kIrqDma0 << c));
                char b2[80];
                std::snprintf(b2, sizeof(b2), "op=irq src=dma%d cyc=%llu", c,
                              (unsigned long long)cyc);
                log(b2);
            }
        }
    }

    // ---- timers ----
    static u64 timer_prescale(int field) {
        switch (field) {
            case 0: return 1;
            case 1: return 64;
            case 2: return 256;
            default: return 1024;
        }
    }

    void schedule_timer(int n) {
        Timer& t = tm[n];
        if (!t.enable() || t.cascade()) return;
        if (tm_armed[n]) return;  // already have exactly one pending event
        tm_armed[n] = true;
        u64 delta = next_overflow_in(t.counter, timer_prescale(t.field()));
        sched.schedule(sched.now + delta, [this, n] {
            tm_armed[n] = false;
            timer_overflow(n);
        });
    }

    void timer_overflow(int n) {
        Timer& t = tm[n];
        t.counter = t.reload;
        char buf[80];
        std::snprintf(buf, sizeof(buf), "op=tmr tm=%d cyc=%llu", n,
                      (unsigned long long)sched.now);
        log(buf);
        if (t.irq_en()) {
            irq.raise(u16(kIrqTimer0 << n));
            char b2[80];
            std::snprintf(b2, sizeof(b2), "op=irq src=t%d cyc=%llu", n,
                          (unsigned long long)sched.now);
            log(b2);
        }
        // Cascade feed into the next stage.
        if (n + 1 < 4 && tm[n + 1].enable() && tm[n + 1].cascade())
            timer_overflow(n + 1);
        schedule_timer(n);  // re-arm
    }

    // ---- script ----
    // Route an IO write into the timing model (DMA/timers/IRQ registers).
    void apply_io_write(u32 addr, u16 value) {
        if (addr >= 0x040000B0u && addr < 0x040000E0u) {
            int c = int((addr - 0x040000B0u) / 12);
            u32 off = (addr - 0x040000B0u) % 12;
            switch (off) {
                case 0: ch[c].sad = (ch[c].sad & 0xFFFF0000u) | value; break;
                case 2:
                    ch[c].sad = (ch[c].sad & 0xFFFFu) | u32(value) << 16;
                    break;
                case 4: ch[c].dad = (ch[c].dad & 0xFFFF0000u) | value; break;
                case 6:
                    ch[c].dad = (ch[c].dad & 0xFFFFu) | u32(value) << 16;
                    break;
                case 8: ch[c].count = value; break;
                case 10: {
                    bool was = ch[c].enable();
                    ch[c].control = value;
                    if (!was && ch[c].enable()) fired[c] = false;
                    if (!ch[c].enable()) fired[c] = false;
                    break;
                }
            }
            return;
        }
        if ((addr & ~3u) == 0x04000100u) {  // TMnCNT_L/H blocks
            int n = int((addr >> 2) & 3);
            if ((addr & 2) == 0) {
                tm[n].reload = value;
            } else {
                bool was = tm[n].enable();
                tm[n].control = value;
                if (!was && tm[n].enable()) tm[n].counter = tm[n].reload;
            }
        } else if (addr == 0x04000200u) {
            irq.ie = value;
        } else if (addr == 0x04000208u) {
            irq.ime = value != 0;
        }
    }

    void schedule_script_write(u32 cycle, u32 addr, u16 value) {
        sched.schedule(cycle, [this, addr, value] {
            bus.wr16(addr, value);
            apply_io_write(addr, value);
            for (int n = 0; n < 4; ++n)
                if (tm[n].enable() && !tm[n].cascade()) schedule_timer(n);
            for (int c = 0; c < 4; ++c)
                if (!ch[c].enable()) fired[c] = false;
            char buf[96];
            std::snprintf(buf, sizeof(buf),
                          "op=wr addr=%08X val=%04X cyc=%llu", addr, value,
                          (unsigned long long)sched.now);
            log(buf);
        });
    }
};

}  // namespace gba
