#pragma once
// GBA DMA channel model: register layout, control decoding, address
// stepping and immediate (CPU-timed) transfers over a minimal bus.
//
// Simplified timing model (documented in SPEC.md): each transferred unit
// costs a fixed number of guest cycles; ordering, not absolute cost, is
// what the tests pin down.
#include <cstdint>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

// Minimal flat bus covering what DMA can legally touch.
struct Bus {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kEwramBase = 0x02000000;  // 256 KiB
    static constexpr u32 kIwramBase = 0x03000000;  // 32 KiB
    static constexpr u32 kPalBase = 0x05000000;

    u8 ewram[0x40000] = {};
    u8 iwram[0x8000] = {};
    u16 io[0x100] = {};  // 0x04000000..FF as halfwords

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
            case 5: return 0;  // palette reads unused here
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

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Byte step per transferred unit for src/dst address control values:
// inc=+size, dec=-size, fixed=0. "Inc+reload" (dst only, value 1) behaves
// like inc during a burst; reload happens between bursts (02/05 exercises).
inline s32 addr_step(int ctl_bits, bool word) {
    s32 size = word ? 4 : 2;
    switch (ctl_bits & 3) {
        case 0: return size;   // increment
        case 1: return size;   // increment + reload (dst)
        case 2: return -size;  // decrement
        default: return 0;     // fixed
    }
}
//@LABS-STUB
// TODO(1): return the signed byte step for one unit. Control values:
// 0/1 increment by the unit size, 2 decrement, 3 fixed. Size is 4 bytes for
// 32-bit transfers and 2 bytes for 16-bit ones.
inline s32 addr_step(int ctl_bits, bool word) {
    (void)ctl_bits;
    (void)word;
    return 0;  // wrong on purpose
}
//@LABS-END

// Raw DMA register file for one channel (SAD, DAD, count, control).
struct DmaRegs {
    u32 sad = 0;
    u32 dad = 0;
    u16 count = 0;
    u16 control = 0;

    bool enable() const { return (control >> 15) & 1; }
    bool irq_en() const { return (control >> 14) & 1; }
    int timing() const { return (control >> 12) & 3; }  // 0 imm,1 vb,2 hb,3 sp
    bool word() const { return (control >> 10) & 1; }
    bool repeat() const { return (control >> 9) & 1; }
    int src_ctl() const { return (control >> 7) & 3; }
    int dst_ctl() const { return (control >> 5) & 3; }

    u32 effective_count() const {
        return count == 0 ? 0x10000u : u32(count);  // full-range units
    }
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Run an immediate transfer. Honors width, both address modes and the full
// 0x10000 wrap of the count field. Clears ENABLE when finished unless the
// channel re-arms via repeat (repeat matters only for triggered timings,
// which callers handle; immediate always clears). Returns cycles consumed:
// our simplified cost model charges 6 cycles per halfword and 8 per word.
inline u64 run_immediate_transfer(Bus& bus, DmaRegs& r, bool& irq_raised) {
    irq_raised = false;
    if (!r.enable()) return 0;
    u64 cycles = 0;
    s32 ss = addr_step(r.src_ctl(), r.word());
    s32 ds = addr_step(r.dst_ctl(), r.word());
    u32 sad = r.sad, dad = r.dad;
    u32 units = r.effective_count();
    for (u32 i = 0; i < units; ++i) {
        if (r.word()) {
            bus.wr32(dad, bus.rd32(sad));
        } else {
            bus.wr16(dad, bus.rd16(sad));
        }
        sad += u32(ss);
        dad += u32(ds);
        cycles += r.word() ? 8 : 6;
    }
    if (r.irq_en()) irq_raised = true;
    if (!r.repeat() || r.timing() == 0) r.control &= ~0x8000u;  // clear ENABLE
    return cycles;
}
//@LABS-STUB
// TODO(2): run an enabled immediate transfer: copy `effective_count()` units
// from SAD to DAD honoring width and address steps, charge 6 cycles per
// halfword / 8 per word, raise `irq_raised` when IRQ-on-complete is set, and
// clear the ENABLE bit afterwards (immediate never repeats).
inline u64 run_immediate_transfer(Bus& bus, DmaRegs& r, bool& irq_raised) {
    (void)bus;
    (void)r;
    irq_raised = false;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace gba
