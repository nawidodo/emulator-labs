#pragma once
// ch39 / 03_memory_map — the PSX CPU bus: segments, mirrors, devices.
//
// Physical map (nocash PSX-SPX "Memory Map"):
//   00000000  2MB Main RAM (mirrored through the first 8MB; RAM_SIZE reg)
//   1F800000  1KB Scratchpad (data cache used as fast RAM; NO KSEG1 mirror)
//   1F801000  Memory-control I/O ports (delay/size regs, RAM_SIZE @+60h)
//   1FC00000  512KB BIOS ROM (normally fetched uncached via KSEG1)
//
// Virtual segments:
//   KUSEG 00000000-7FFFFFFF : cached mirror of the first 512MB
//   KSEG0 80000000-9FFFFFFF : cached mirror (kernel)
//   KSEG1 A0000000-BFFFFFFF : UNcached mirror (kernel) — BIOS/RAM init runs here
//   KSEG2 C0000000-FFFFFFFF : kernel TLB space on real MIPS; only FFFE0000
//                             (cache control) exists on PSX -> fault here.
//
// Reference: https://problemkaputt.de/psx-spx.htm#memorymap

#include <cstdint>
#include <span>
#include <vector>

namespace psx::r3000a {

enum class Segment { Kuseg, Kseg0, Kseg1, Kseg2 };

enum class CacheAttr { Cached, Uncached };

constexpr uint32_t kRamSize = 2u * 1024 * 1024;
constexpr uint32_t kScratchpadSize = 1024;
constexpr uint32_t kScratchpadBase = 0x1F800000u;
constexpr uint32_t kMemCtlBase = 0x1F801000u;
constexpr uint32_t kBiosSize = 512u * 1024;
constexpr uint32_t kBiosBase = 0x1FC00000u;

struct Bus {
    std::vector<uint8_t> ram = std::vector<uint8_t>(kRamSize, 0);
    std::vector<uint8_t> scratchpad = std::vector<uint8_t>(kScratchpadSize, 0);
    std::vector<uint8_t> bios = std::vector<uint8_t>(kBiosSize, 0xFF);

    // Memory-control ports. Defaults are the reset values documented in
    // PSX-SPX "Memory Control"; the BIOS rewrites some during init.
    struct MemCtl {
        uint32_t exp1_base = 0x1F000000u;
        uint32_t exp2_base = 0x1F802000u;
        uint32_t exp1_delay_size = 0x0013243Fu;
        uint32_t exp3_delay_size = 0x00003022u;
        uint32_t bios_delay_size = 0x0013243Fu;
        uint32_t spu_delay = 0x200931E1u;
        uint32_t cdrom_delay = 0x00020843u;
        uint32_t exp2_delay_size = 0x00070777u;
        uint32_t com_delay = 0x00031125u;
        // Reset value 00000B88h: bits 11:9 = 5 -> 8MB decode window holding
        // the 2MB chips repeated four times (BIOS later writes 00000888h).
        uint32_t ram_size = 0x00000B88u;
        uint32_t i_stat = 0;  // interrupt controller lives in ch40
        uint32_t i_mask = 0;
    } memctl{};

    void load_bios(std::span<const uint8_t> image) {
        const uint32_t n =
            static_cast<uint32_t>(image.size()) < kBiosSize
                ? static_cast<uint32_t>(image.size())
                : kBiosSize;
        for (uint32_t i = 0; i < n; ++i) bios[i] = image[i];
    }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline Segment segment_of(uint32_t vaddr) {
    if (vaddr < 0x80000000u) return Segment::Kuseg;
    if (vaddr < 0xA0000000u) return Segment::Kseg0;
    if (vaddr < 0xC0000000u) return Segment::Kseg1;
    return Segment::Kseg2;
}

// All of KUSEG/KSEG0/KSEG1 alias the same physical first-512MB window; the
// low 29 bits ARE the physical address. This single AND is the whole "MMU"
// of the PlayStation.
inline constexpr uint32_t physical_address(uint32_t vaddr) {
    return vaddr & 0x1FFFFFFFu;
}

inline CacheAttr cache_attr(Segment seg) {
    // Only KSEG1 bypasses the caches; that is why BIOS code lives at
    // BFC00000 and why early boot code is deterministic cycle-for-cycle.
    return seg == Segment::Kseg1 ? CacheAttr::Uncached : CacheAttr::Cached;
}
//@LABS-STUB
// TODO(1): classify a virtual address into its segment, strip the segment
// bits to get the physical address, and report per-segment cacheability.
inline Segment segment_of(uint32_t vaddr) {
    (void)vaddr;
    return Segment::Kuseg;  // wrong on purpose
}
inline constexpr uint32_t physical_address(uint32_t vaddr) {
    return vaddr;  // wrong on purpose: no segment masking
}
inline CacheAttr cache_attr(Segment seg) {
    (void)seg;
    return CacheAttr::Cached;  // wrong on purpose: KSEG1 is uncached
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Device selection by physical address. Returns false when nothing decodes
// the address (locked RAM windows beyond 8MB, expansion regions, ...).
inline bool decode(uint32_t paddr, uint32_t* dev_base) {
    if (paddr < 0x00800000u) {  // RAM mirrored in the first 8MB window
        *dev_base = 0;
        return true;
    }
    if (paddr >= kScratchpadBase && paddr < kScratchpadBase + kScratchpadSize) {
        *dev_base = kScratchpadBase;
        return true;
    }
    if (paddr >= kMemCtlBase && paddr < kMemCtlBase + 0x80u) {
        *dev_base = kMemCtlBase;  // through I_MASK @1F801074
        return true;
    }
    if (paddr >= kBiosBase && paddr < kBiosBase + kBiosSize) {
        *dev_base = kBiosBase;
        return true;
    }
    return false;
}

// RAM repeats every 2MB inside the 8MB decode window (four /RAS chips see
// the same data lines), so mask AFTER decoding.
inline uint32_t ram_offset(uint32_t paddr) { return paddr & (kRamSize - 1); }

inline uint32_t off_of(uint32_t paddr, uint32_t base) {
    return base == 0 ? ram_offset(paddr) : paddr - base;
}

template <typename T>
bool bus_read(const Bus* bus, uint32_t vaddr, T* out,
              CacheAttr* attr = nullptr) {
    const Segment seg = segment_of(vaddr);
    if (attr) *attr = cache_attr(seg);
    if (seg == Segment::Kseg2) return false;
    const uint32_t paddr = physical_address(vaddr);
    uint32_t base = 0;
    if (!decode(paddr, &base)) return false;
    // PSX-SPX memory map: the scratchpad has NO KSEG1 column — A0000000-
    // segment aliases of 1F800000 simply do not decode.
    if (base == kScratchpadBase && seg == Segment::Kseg1) return false;

    if (base == kMemCtlBase) {
        if constexpr (sizeof(T) != 4) {
            return false;  // port model is word-granular
        } else {
            const Bus::MemCtl& m = bus->memctl;
            switch ((paddr - base) >> 2) {
                case 0x00: *out = m.exp1_base; break;
                case 0x01: *out = m.exp2_base; break;
                case 0x02: *out = m.exp1_delay_size; break;
                case 0x03: *out = m.exp3_delay_size; break;
                case 0x04: *out = m.bios_delay_size; break;
                case 0x05: *out = m.spu_delay; break;
                case 0x06: *out = m.cdrom_delay; break;
                case 0x07: *out = m.exp2_delay_size; break;
                case 0x08: *out = m.com_delay; break;
                case 0x18: *out = m.ram_size; break;   // 1F801060
                case 0x1C: *out = m.i_stat; break;     // 1F801070
                case 0x1D: *out = m.i_mask; break;     // 1F801074
                default: *out = 0; break;              // unmapped port reads 0
            }
            return true;
        }
    }

    const uint8_t* dev =
        base == 0                 ? bus->ram.data()
        : base == kScratchpadBase ? bus->scratchpad.data()
                                  : bus->bios.data();
    const uint8_t* src = dev + off_of(paddr, base);
    if constexpr (sizeof(T) == 4) {
        *out = static_cast<T>(src[0]) | static_cast<T>(src[1]) << 8 |
               static_cast<T>(src[2]) << 16 | static_cast<T>(src[3]) << 24;
    } else if constexpr (sizeof(T) == 2) {
        *out = static_cast<T>(src[0]) | static_cast<T>(src[1]) << 8;
    } else {
        *out = static_cast<T>(src[0]);
    }
    return true;
}
//@LABS-STUB
// TODO(2): device decode + little-endian reads. Decode order: RAM window
// (first 8MB, wraps every 2MB), scratchpad 1KB @1F800000, memory control
// ports @1F801000-3F, BIOS ROM 512KB @1FC00000. KSEG2 never decodes.
inline bool decode(uint32_t paddr, uint32_t* dev_base) {
    (void)paddr; (void)dev_base;
    return false;  // wrong on purpose
}

inline uint32_t ram_offset(uint32_t paddr) {
    return paddr;  // wrong on purpose: no wrap masking
}

inline uint32_t off_of(uint32_t paddr, uint32_t base) {
    (void)base;
    return paddr;  // wrong on purpose
}

template <typename T>
bool bus_read(const Bus*, uint32_t, T*, CacheAttr* = nullptr) {
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
template <typename T>
bool bus_write(Bus* bus, uint32_t vaddr, T value, CacheAttr* attr = nullptr) {
    const Segment seg = segment_of(vaddr);
    if (attr) *attr = cache_attr(seg);
    if (seg == Segment::Kseg2) return false;
    const uint32_t paddr = physical_address(vaddr);
    uint32_t base = 0;
    if (!decode(paddr, &base)) return false;
    if (base == kScratchpadBase && seg == Segment::Kseg1) return false;

    if (base == kMemCtlBase && sizeof(T) == 4) {
        Bus::MemCtl* m = &bus->memctl;
        switch (paddr - base) {
            case 0x00: m->exp1_base = value; break;
            case 0x04: m->exp2_base = value; break;
            case 0x08: m->exp1_delay_size = value; break;
            case 0x0C: m->exp3_delay_size = value; break;
            case 0x10: m->bios_delay_size = value; break;
            case 0x14: m->spu_delay = value; break;
            case 0x18: m->cdrom_delay = value; break;
            case 0x1C: m->exp2_delay_size = value; break;
            case 0x20: m->com_delay = value; break;
            case 0x60: m->ram_size = value; break;
            case 0x70: m->i_stat = value; break;
            case 0x74: m->i_mask = value; break;
            default: break;  // unknown port swallows the write like hardware
        }
        return true;
    }
    if (base == kBiosBase) return false;  // ROM: writes silently lost

    uint8_t* dev = base == 0 ? bus->ram.data() : bus->scratchpad.data();
    uint8_t* dst = dev + off_of(paddr, base);
    if constexpr (sizeof(T) == 4) {
        dst[0] = value & 0xFF;
        dst[1] = (value >> 8) & 0xFF;
        dst[2] = (value >> 16) & 0xFF;
        dst[3] = (value >> 24) & 0xFF;
    } else if constexpr (sizeof(T) == 2) {
        dst[0] = value & 0xFF;
        dst[1] = (value >> 8) & 0xFF;
    } else {
        dst[0] = value & 0xFF;
    }
    return true;
}
//@LABS-STUB
// TODO(3): little-endian writes. RAM/scratchpad store bytes; memory-control
// ports capture words; BIOS ignores writes (ROM).
template <typename T>
bool bus_write(Bus*, uint32_t, T, CacheAttr* = nullptr) {
    return false;  // wrong on purpose
}
//@LABS-END

}  // namespace psx::r3000a
