#pragma once
// GBA bitmap display modes (3/4/5) over a flat PPU memory model.
//
// The memory model is deliberately honest about the GBA's 16-bit video bus:
// all VRAM/palette/OAM accesses are u16-sized, so 8bpp pixels are always
// packed two-per-u16 on disk. See LECTURE.md for register layout.
#include <cstdint>
#include <cstring>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using u64 = uint64_t;
using u32 = uint32_t;
using s32 = int32_t;

constexpr int kScreenW = 240;
constexpr int kScreenH = 160;

// Flat PPU memory model holding only what video hardware can touch.
struct PpuMemory {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kPalBase = 0x05000000;
    static constexpr u32 kVramBase = 0x06000000;
    static constexpr u32 kOamBase = 0x07000000;

    u8 io[0x100] = {};     // 04000000..040000FF
    u8 pal[0x400] = {};    // 05000000..050003FF (512 u16 entries)
    u8 vram[0x18000] = {}; // 06000000..06xxxx (96 KiB)
    u8 oam[0x400] = {};    // 07000000..070003FF

    void reset() { *this = PpuMemory{}; }

    static u16 rd_le(const u8* p) { return u16(p[0]) | u16(p[1]) << 8; }
    static void wr_le(u8* p, u16 v) {
        p[0] = u8(v);
        p[1] = u8(v >> 8);
    }

    // Aligned 16-bit access only; bit 0 of every address is ignored, exactly
    // like the real bus.
    u16 rd16(u32 addr) const {
        switch ((addr >> 24) & 7) {
            case 4: return rd_le(io + ((addr - kIoBase) & 0xFE));
            case 5: return rd_le(pal + (addr & 0x3FE));
            case 6: return rd_le(vram + (addr & 0x17FFE));
            case 7: return rd_le(oam + (addr & 0x3FE));
            default: return 0;
        }
    }
    void wr16(u32 addr, u16 v) {
        switch ((addr >> 24) & 7) {
            case 4: wr_le(io + ((addr - kIoBase) & 0xFE), v); break;
            case 5: wr_le(pal + (addr & 0x3FE), v); break;
            case 6: wr_le(vram + (addr & 0x17FFE), v); break;
            case 7: wr_le(oam + (addr & 0x3FE), v); break;
            default: break;
        }
    }

    u16 dispcnt() const { return rd16(kIoBase); }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Bit-replicating BGR555 -> RGBA8888: (v<<3)|(v>>2) sends 31 to exactly 255,
// so pure white/black never round wrong. Output layout is host RGBA order.
inline u32 bgr555_to_rgba8888(u16 c) {
    u32 r5 = c & 31u;
    u32 g5 = (c >> 5) & 31u;
    u32 b5 = (c >> 10) & 31u;
    u32 r = (r5 << 3) | (r5 >> 2);
    u32 g = (g5 << 3) | (g5 >> 2);
    u32 b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}
//@LABS-STUB
// TODO(1): convert a BGR555 u16 to opaque RGBA8888 using bit replication
// ((v5 << 3) | (v5 >> 2) per channel). Stub returns transparent black.
inline u32 bgr555_to_rgba8888(u16 c) {
    (void)c;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Mode 3: one u16 BGR555 per pixel, stride 240 from the VRAM base.
inline u32 mode3_vram_offset(int x, int y) {
    return u32(y) * kScreenW + u32(x);
}
inline void mode3_set_pixel(PpuMemory& m, int x, int y, u16 color555) {
    if (unsigned(x) >= unsigned(kScreenW) || unsigned(y) >= unsigned(kScreenH))
        return;
    m.wr16(PpuMemory::kVramBase + mode3_vram_offset(x, y) * 2, color555);
}
inline u16 mode3_get_pixel(const PpuMemory& m, int x, int y) {
    if (unsigned(x) >= unsigned(kScreenW) || unsigned(y) >= unsigned(kScreenH))
        return 0;
    return m.rd16(PpuMemory::kVramBase + mode3_vram_offset(x, y) * 2);
}
//@LABS-STUB
// TODO(2): implement mode 3 pixel store/load. Offset into VRAM is
// (y * 240 + x) * 2 bytes from 0x06000000; ignore out-of-range writes.
inline u32 mode3_vram_offset(int x, int y) {
    (void)x;
    (void)y;
    return 0;  // wrong on purpose
}
inline void mode3_set_pixel(PpuMemory& m, int x, int y, u16 color555) {
    (void)m;
    (void)x;
    (void)y;
    (void)color555;
}
inline u16 mode3_get_pixel(const PpuMemory& m, int x, int y) {
    (void)m;
    (void)x;
    (void)y;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Mode 4: 8bpp palette indices, two pages (DISPCNT bit 4 selects which one
// the PPU displays; games draw into the other and flip on VBlank).
inline u32 mode4_page_base(u16 dispcnt) {
    return (dispcnt & 0x10u) ? 0xA000u : 0u;
}
inline void mode4_set_pixel(PpuMemory& m, int x, int y, u8 index) {
    if (unsigned(x) >= unsigned(kScreenW) || unsigned(y) >= unsigned(kScreenH))
        return;
    u32 byte = mode4_page_base(m.dispcnt()) + u32(y) * kScreenW + u32(x);
    u32 aligned = PpuMemory::kVramBase + (byte & ~1u);
    u16 word = m.rd16(aligned);
    if (byte & 1)
        word = u16((word & 0x00FFu) | (u16(index) << 8));
    else
        word = u16((word & 0xFF00u) | index);
    m.wr16(aligned, word);
}
inline u8 mode4_get_pixel(const PpuMemory& m, int x, int y) {
    if (unsigned(x) >= unsigned(kScreenW) || unsigned(y) >= unsigned(kScreenH))
        return 0;
    u32 byte = mode4_page_base(m.dispcnt()) + u32(y) * kScreenW + u32(x);
    u16 word = m.rd16(PpuMemory::kVramBase + (byte & ~1u));
    return (byte & 1) ? u8(word >> 8) : u8(word);
}
//@LABS-STUB
// TODO(3): implement mode 4 page selection and packed 8-bit pixels.
// Page base is 0xA000 when DISPCNT bit 4 is set else 0; pixel byte lives at
// base + y*240 + x and must be patched inside its containing u16.
inline u32 mode4_page_base(u16 dispcnt) {
    (void)dispcnt;
    return 0;  // TODO(3): honor the page bit
}
inline void mode4_set_pixel(PpuMemory& m, int x, int y, u8 index) {
    (void)m;
    (void)x;
    (void)y;
    (void)index;
}
inline u8 mode4_get_pixel(const PpuMemory& m, int x, int y) {
    (void)m;
    (void)x;
    (void)y;
    return 0;  // wrong on purpose
}
//@LABS-END

// Geometry descriptor for the active bitmap mode.
struct BitmapModeInfo {
    int w = 0;
    int h = 0;
    bool paletted = false;  // true for mode 4 (8bpp indices), false = direct 555
};

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Geometry per mode; the page base comes from mode4_page_base().
inline BitmapModeInfo bitmap_mode_info(int mode) {
    switch (mode) {
        case 3: return {240, 160, false};
        case 4: return {240, 160, true};
        case 5: return {160, 128, false};  // small frame, top-left anchored
        default: return {0, 0, false};     // modes 6/7 do not exist
    }
}
inline bool forced_blank(u16 dispcnt) { return (dispcnt >> 6) & 1u; }
//@LABS-STUB
// TODO(4): fill in bitmap geometry (mode 3: 240x160 direct, mode 4: 240x160
// paletted, mode 5: 160x128 direct) and the forced-blank flag (bit 6).
inline BitmapModeInfo bitmap_mode_info(int mode) {
    (void)mode;
    return {0, 0, false};  // wrong on purpose
}
inline bool forced_blank(u16 dispcnt) {
    (void)dispcnt;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Render one full frame of the active bitmap mode to RGBA8888 (240x160,
// row-major). Outside mode 5's small frame the backdrop color PAL[0] shows;
// during forced blank the screen is white.
inline void render_bitmap_frame(const PpuMemory& m, u32* out) {
    u16 dc = m.dispcnt();
    BitmapModeInfo info = bitmap_mode_info(dc & 7);
    u32 page = (dc & 7) == 3 ? 0 : mode4_page_base(dc);
    for (int y = 0; y < kScreenH; ++y) {
        for (int x = 0; x < kScreenW; ++x) {
            u32 px;
            if (forced_blank(dc)) {
                px = 0xFFFFFFFFu;
            } else if (x < info.w && y < info.h) {
                if (info.paletted) {
                    u16 entry =
                        m.rd16(PpuMemory::kPalBase +
                               u32(mode4_get_pixel(m, x, y)) * 2);
                    px = bgr555_to_rgba8888(entry);
                } else {
                    u32 base = PpuMemory::kVramBase + page;
                    px = bgr555_to_rgba8888(
                        m.rd16(base + (u32(y) * info.w + u32(x)) * 2));
                }
            } else {
                px = bgr555_to_rgba8888(
                    m.rd16(PpuMemory::kPalBase));  // backdrop PAL[0]
            }
            out[u32(y) * kScreenW + u32(x)] = px;
        }
    }
}
//@LABS-STUB
// TODO(5): render a whole frame. Loop 240x160, consult bitmap_mode_info and
// the page base; forced blank renders white; mode 5 shows backdrop PAL[0]
// outside its 160x128 frame. Stub leaves the buffer untouched.
inline void render_bitmap_frame(const PpuMemory& m, u32* out) {
    (void)m;
    (void)out;
    // TODO(5): implement
}
//@LABS-END

// FNV-1a 64 over raw bytes — same digest as tools/labs/hash_frame.py.
inline u64 fnv64(const void* data, size_t n) {
    u64 h = 0xCBF29CE484222325ull;
    const u8* p = static_cast<const u8*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace gba
