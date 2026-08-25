#pragma once
// GBA DMA trigger handling: HBlank/VBlank/sound-FIFO timing, repeat
// re-arming, FIFO destination reload, and same-instant arbitration.
//
// Video timing constants are the GBA's: 1232 cycles per scanline, HBlank
// begins 960 cycles into a line, VBlank spans lines 160..226.
#include <cstdint>
#include <vector>

// Bus + DMA register model duplicated from 01_dma_channels so this
// exercise builds standalone.
namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

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
    u32 rd32(u32 addr) const {
        return u32(rd16(addr)) | u32(rd16(addr + 2)) << 16;
    }
    void wr32(u32 addr, u32 v) {
        wr16(addr, u16(v));
        wr16(addr + 2, u16(v >> 16));
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

inline s32 addr_step(int ctl_bits, bool word) {
    s32 size = word ? 4 : 2;
    switch (ctl_bits & 3) {
        case 0:
        case 1: return size;
        case 2: return -size;
        default: return 0;
    }
}

// Provided: immediate transfer engine from ex.01 (plain code here).
inline u64 run_immediate_transfer(Bus& bus, DmaRegs& r, bool& irq_raised) {
    irq_raised = false;
    if (!r.enable()) return 0;
    u64 cycles = 0;
    s32 ss = addr_step(r.src_ctl(), r.word());
    s32 ds = addr_step(r.dst_ctl(), r.word());
    u32 sad = r.sad, dad = r.dad;
    for (u32 i = 0; i < r.effective_count(); ++i) {
        if (r.word())
            bus.wr32(dad, bus.rd32(sad));
        else
            bus.wr16(dad, bus.rd16(sad));
        sad += u32(ss);
        dad += u32(ds);
        cycles += r.word() ? 8 : 6;
    }
    if (r.irq_en()) irq_raised = true;
    if (!r.repeat() || r.timing() == 0) r.control &= ~0x8000u;
    return cycles;
}

}  // namespace gba

namespace gba {

constexpr u32 kCyclesPerLine = 1232;
constexpr u32 kHblankStart = 960;
constexpr u32 kVblankLine = 160;
constexpr u32 kTotalLines = 228;

enum class Trigger { Immediate, VBlank, HBlank, Special };

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Should this channel fire on `trig`? Immediate channels never answer
// video/FIFO events; triggered channels ignore immediate requests. A
// non-repeat channel fires at most once per enable (callers track
// `already_fired`). Special-timing channels only respond to their FIFO /
// capture event, which callers pass as Trigger::Special.
inline bool dma_should_fire(const DmaRegs& r, Trigger trig,
                            bool already_fired) {
    if (!r.enable()) return false;
    Trigger want = Trigger(r.timing());
    if (want != trig) return false;
    if (want == Trigger::Immediate) return false;  // handled by CPU loop
    if (!r.repeat() && already_fired) return false;
    return true;
}
//@LABS-STUB
// TODO(1): decide whether a channel answers this trigger. Rules: channel
// must be enabled; its configured timing must match; immediate channels
// never answer event triggers here; without repeat a triggered channel
// fires only once per enable (`already_fired`).
inline bool dma_should_fire(const DmaRegs& r, Trigger trig,
                            bool already_fired) {
    (void)r;
    (void)trig;
    (void)already_fired;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Arbitration for channels requesting in the same time slice: fixed
// priority DMA0 > DMA1 > DMA2 > DMA3. Input is an unordered list of pending
// channel numbers; output is service order. (A running DMA is never
// interrupted by a lower channel — modeled by the scheduler in ex.05.)
inline std::vector<int> arbitrate(const std::vector<int>& pending) {
    std::vector<int> order = pending;
    for (size_t i = 0; i + 1 < order.size(); ++i)
        for (size_t j = i + 1; j < order.size(); ++j)
            if (order[j] < order[i]) std::swap(order[i], order[j]);
    return order;
}
//@LABS-STUB
// TODO(2): sort pending channel numbers into hardware service order:
// lowest channel number first (DMA0 has absolute priority over DMA3).
inline std::vector<int> arbitrate(const std::vector<int>& pending) {
    (void)pending;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Sound-FIFO refill: transfer four words from SAD into the modeled FIFO
// buffer (four consecutive words starting at fifo_addr) and reload DAD to
// its base so the next burst starts over (hardware behavior for FIFO
// channels regardless of the dst-control bits). Returns cycles consumed.
inline u64 fifo_refill(Bus& bus, DmaRegs& r, u32 fifo_addr) {
    u64 cycles = 0;
    u32 sad = r.sad;
    for (int i = 0; i < 4; ++i) {
        bus.wr32(fifo_addr + u32(i) * 4, bus.rd32(sad));
        sad += 4;
        cycles += 8;
    }
    r.dad = fifo_addr;  // reload to base
    return cycles;
}
//@LABS-STUB
// TODO(3): perform one sound-FIFO refill: copy exactly four 32-bit words
// from SAD (advancing source) to `fifo_addr`, charge 8 cycles per word,
// then reset DAD back to `fifo_addr` so subsequent bursts re-align.
inline u64 fifo_refill(Bus& bus, DmaRegs& r, u32 fifo_addr) {
    (void)bus;
    (void)r;
    (void)fifo_addr;
    return 0;  // wrong on purpose
}
//@LABS-END

// One dispatched action, for tests and traces.
struct DmaAction {
    int channel = -1;
    u64 cycle = 0;
};

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Dispatch one scanline boundary event: at a line's HBlank instant run all
// HBlank-triggered channels; at the first VBlank line run VBlank channels.
// `fired[]` tracks once-per-enable state. Channels run in arbitration
// order and each consumes its simplified cycle cost starting at `cycle`.
inline std::vector<DmaAction> run_video_event(Bus& bus, DmaRegs (&ch)[4],
                                              bool (&fired)[4],
                                              Trigger trig, u64 cycle) {
    std::vector<DmaAction> actions;
    std::vector<int> pending;
    for (int i = 0; i < 4; ++i)
        if (ch[i].enable() && dma_should_fire(ch[i], trig, fired[i]))
            pending.push_back(i);
    bool irq_unused = false;
    (void)irq_unused;
    for (int c : arbitrate(pending)) {
        run_immediate_transfer(bus, ch[c], irq_unused);
        fired[c] = true;
        actions.push_back({c, cycle});
    }
    return actions;
}
//@LABS-STUB
// TODO(4): dispatch a video event: collect enabled channels matching this
// trigger (respecting once-per-enable), serve them in arbitration order,
// run each transfer, mark them fired, and report `{channel, cycle}` actions.
inline std::vector<DmaAction> run_video_event(Bus& bus, DmaRegs (&ch)[4],
                                              bool (&fired)[4],
                                              Trigger trig, u64 cycle) {
    (void)bus;
    (void)ch;
    (void)fired;
    (void)trig;
    (void)cycle;
    return {};  // wrong on purpose
}
//@LABS-END

}  // namespace gba
