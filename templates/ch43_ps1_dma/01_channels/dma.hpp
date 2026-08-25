#pragma once
//
// ch43 / 01_channels — PS1 DMA controller register model
// (psx-spx section "DMA Controllers", ports 1F80108xh..1F8010F4h).
//
// The PS1 DMA unit has seven fixed channels, each owning three registers
// (MADR/BCR/CHCR). Channel order is fixed by hardware and matters for both
// priority arbitration and interrupt-flag bit positions:
//
//   0 MDECin   1 MDECout   2 GPU   3 CDROM   4 SPU   5 PIO   6 OTC

#include <cstdint>

namespace ps1 {

enum class Channel : unsigned {
    MdecIn = 0,
    MdecOut = 1,
    Gpu = 2,
    Cdrom = 3,
    Spu = 4,
    Pio = 5,
    Otc = 6,
};

constexpr unsigned kChannelCount = 7;

// Per-channel register file at 1F801080h + n*10h.
struct ChannelRegs {
    uint32_t madr = 0;  // transfer base address (word aligned)
    uint32_t bcr = 0;   // [31:16] block count, [15:0] words per block
    uint32_t chcr = 0;  // channel control — see chcr_* accessors below
};

// ---- CHCR field accessors (psx-spx layout) ------------------------------
inline unsigned bcr_word_count(uint32_t bcr) { return bcr & 0xFFFFu; }
inline unsigned bcr_block_count(uint32_t bcr) { return bcr >> 16; }

enum SyncMode : unsigned { SyncBurst = 0, SyncSlice = 1, SyncLinkedList = 2 };

inline bool chcr_from_ram(uint32_t chcr) { return (chcr >> 0) & 1u; }
inline unsigned chcr_sync_mode(uint32_t chcr) { return (chcr >> 8) & 0x3u; }
// Chopping window fields: encoded 0..7, decoded as (n+1) units; a zero
// field means chopping is disabled for that side.
inline unsigned chcr_dma_window_field(uint32_t chcr) { return (chcr >> 16) & 0x7u; }
inline unsigned chcr_cpu_window_field(uint32_t chcr) { return (chcr >> 20) & 0x7u; }
inline bool chcr_start_busy(uint32_t chcr) { return (chcr >> 24) & 1u; }
inline bool chcr_force_trigger(uint32_t chcr) { return (chcr >> 28) & 1u; }

class DmaController {
public:
    DmaController() = default;

    ChannelRegs& channel(unsigned ch) { return channels_[ch]; }
    const ChannelRegs& channel(unsigned ch) const { return channels_[ch]; }

    uint32_t dpcr() const { return dpcr_; }
    void set_dpcr(uint32_t v) { dpcr_ = v; }

    uint32_t dicr() const { return dicr_; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Each channel owns one 4-bit DPCR nibble: [3] enable, [2:0] priority
    // (7 = highest). Arbitration among simultaneously-enabled channels
    // uses this priority; ties resolve toward the lower channel number.
    unsigned priority(unsigned ch) const {
        return (dpcr_ >> (ch * 4)) & 0x7u;
    }
//@LABS-STUB
    // TODO(1): extract the 3-bit priority field of `ch`'s DPCR nibble.
    unsigned priority(unsigned ch) const {
        (void)ch;
        return 0;  // wrong on purpose: reports minimum priority always
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // A channel may run only when BOTH its DPCR master-enable bit is set
    // AND software raised the CHCR start bit (or used the force-trigger
    // shortcut that OTC/GPU drivers rely on).
    bool channel_enabled(unsigned ch) const {
        const bool dpcr_en = ((dpcr_ >> (ch * 4 + 3)) & 1u) != 0;
        const bool start = chcr_start_busy(channels_[ch].chcr);
        const bool force = chcr_force_trigger(channels_[ch].chcr);
        return dpcr_en && (start || force);
    }
//@LABS-STUB
    // TODO(2): gate on the DPCR enable bit (bit 3 of the channel nibble)
    // AND (CHCR start bit 24 OR force-trigger bit 28). Must be false when
    // the DPCR nibble has no enable bit even if CHCR says start.
    bool channel_enabled(unsigned ch) const {
        (void)ch;
        return true;  // wrong on purpose: disabled channels would transfer
    }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Register decode for 1F80108xh..1F8010F4h. Unmapped slots read as
    // FFFFFFFFh (open bus) and ignore writes, like real hardware.
    uint32_t read_reg(uint32_t addr) const {
        const uint32_t off = (addr & 0xFFu) - 0x80u;  // base 1F801080h
        if (off < 0x70u) {
            const auto& r = channels_[(off >> 4) & 7u];
            switch (off & 0xFu) {
                case 0x0: return r.madr;
                case 0x4: return r.bcr;
                case 0x8: return r.chcr;
                default:  return 0xFFFFFFFFu;
            }
        }
        switch (off) {
            case 0x70: return dpcr_;   // 1F8010F0h
            case 0x74: return dicr();  // 1F8010F4h
            default:   return 0xFFFFFFFFu;
        }
    }

    void write_reg(uint32_t addr, uint32_t v) {
        const uint32_t off = (addr & 0xFFu) - 0x80u;  // base 1F801080h
        if (off < 0x70u) {
            auto& r = channels_[(off >> 4) & 7u];
            switch (off & 0xFu) {
                case 0x0: r.madr = v; break;
                case 0x4: r.bcr = v; break;
                case 0x8: r.chcr = v; break;
                default: break;
            }
            return;
        }
        if (off == 0x70) {
            dpcr_ = v;
        } else if (off == 0x74) {
            write_dicr(v);
        }
    }
//@LABS-STUB
    // TODO(3): decode MADR/BCR/CHCR at 1F80108xh+{0,4,8}, DPCR at
    // 1F8010F0h, DICR at 1F8010F4h. Unknown addresses must read as
    // 0xFFFFFFFF and drop writes.
    uint32_t read_reg(uint32_t addr) const {
        (void)addr;
        return 0xFFFFFFFFu;  // wrong on purpose
    }
    void write_reg(uint32_t addr, uint32_t v) {
        (void)addr;
        (void)v;
    }
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // Completion flags live at DICR bits 24..29 (one per channel). They
    // are set by hardware on transfer completion regardless of enables;
    // software clears them by WRITING ONES to those bit positions.
    void set_completion_flag(unsigned ch) {
        dicr_ |= (1u << (24 + ch));
    }
    void acknowledge(uint32_t mask) {
        dicr_ &= ~(mask & 0x3F000000u);
    }
//@LABS-STUB
    // TODO(4): implement flag set (bit 24+ch) and acknowledge (clear only
    // bits 24..29 where `mask` has ones).
    void set_completion_flag(unsigned ch) {
        (void)ch;
    }
    void acknowledge(uint32_t mask) {
        (void)mask;
    }
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
    bool irq_active() const {
        if (((dicr_ >> 30) & 1u) == 0) return false;
        // Channels 0..5 carry IRQ bits; OTC (6) never raises DMA
        // interrupts on real hardware.
        for (unsigned ch = 0; ch + 1 < kChannelCount; ++ch) {
            const bool flag = ((dicr_ >> (24 + ch)) & 1u) != 0;
            const bool force = ((dicr_ >> ch) & 1u) != 0;
            const bool en = ((dicr_ >> (16 + ch)) & 1u) != 0;
            if ((flag || force) && en) return true;
        }
        return false;
    }

private:
    void write_dicr(uint32_t v) {
        // Completion flags (bits 24..29) clear where `v` carries ones;
        // force bits, enables and the master bit take the written value
        // directly (a dropped master bit must actually clear).
        dicr_ = (dicr_ & 0x3F000000u & ~(v & 0x3F000000u))
              | (v & 0x407FF1FFu);
    }
//@LABS-STUB
    // TODO(5): implement irq_active() per the documented rule above.
    bool irq_active() const {
        return false;  // wrong on purpose: interrupts never fire
    }

private:
    void write_dicr(uint32_t v) {
        (void)v;
    }
//@LABS-END

    ChannelRegs channels_[kChannelCount]{};
    uint32_t dpcr_ = 0x07654321u;  // hardware reset default
    uint32_t dicr_ = 0;
};

}  // namespace ps1
