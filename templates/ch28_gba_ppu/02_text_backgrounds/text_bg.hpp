#pragma once
// GBA text backgrounds (modes 0/1): BG control words, screen entries,
// 4bpp/8bpp tiles, flips, scroll wrap-around and priority ordering.
//
// The memory model mirrors 01_bitmap_modes but each exercise directory
// compiles on its own.
#include <cstdint>
#include <vector>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr int kScreenW = 240;
constexpr int kScreenH = 160;
constexpr u32 kVramSize = 0x18000;

struct PpuMemory {
    static constexpr u32 kIoBase = 0x04000000;
    static constexpr u32 kPalBase = 0x05000000;
    static constexpr u32 kVramBase = 0x06000000;

    u8 io[0x100] = {};
    u8 pal[0x400] = {};
    u8 vram[kVramSize] = {};

    void reset() { *this = PpuMemory{}; }

    static u16 rd_le(const u8* p) { return u16(p[0]) | u16(p[1]) << 8; }
    static void wr_le(u8* p, u16 v) {
        p[0] = u8(v);
        p[1] = u8(v >> 8);
    }

    u16 rd16(u32 addr) const {
        switch ((addr >> 24) & 7) {
            case 4: return rd_le(io + ((addr - kIoBase) & 0xFE));
            case 6:
                return rd_le(vram + (((addr - kVramBase) % kVramSize) &
                                     ~1u));
            default: return 0;
        }
    }
    void wr16(u32 addr, u16 v) {
        switch ((addr >> 24) & 7) {
            case 4: wr_le(io + ((addr - kIoBase) & 0xFE), v); break;
            case 5: wr_le(pal + (addr & 0x3FE), v); break;
            case 6:
                wr_le(vram + (((addr - kVramBase) % kVramSize) & ~1u), v);
                break;
            default: break;
        }
    }
    u16 dispcnt() const { return rd16(kIoBase); }
};

// BGnCNT (0x04000008 + 2*n): priority bits 0-1, char base bits 2-3 in
// 16 KiB units, mosaic bit 6, 8bpp bit 7, screen base bits 8-12 in 2 KiB
// units, size bits 14-15 -> map size in tiles (256/512 px per axis).
struct TextBgConfig {
    int priority = 0;      // 0 = frontmost
    int char_base = 0;     // byte offset of tile pool
    bool mosaic = false;
    bool bpp8 = false;     // else 4bpp with 16-color banks
    int screen_base = 0;   // byte offset of screen map
    int map_w_tiles = 32;
    int map_h_tiles = 32;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline TextBgConfig decode_text_bg_config(u16 cnt) {
    static constexpr int kW[] = {32, 64, 32, 64};
    static constexpr int kH[] = {32, 32, 64, 64};
    TextBgConfig c;
    c.priority = cnt & 3;
    c.char_base = ((cnt >> 2) & 3) * 0x4000;
    c.mosaic = (cnt >> 6) & 1;
    c.bpp8 = (cnt >> 7) & 1;
    c.screen_base = ((cnt >> 8) & 31) * 0x800;
    int size = (cnt >> 14) & 3;
    c.map_w_tiles = kW[size];
    c.map_h_tiles = kH[size];
    return c;
}
//@LABS-STUB
// TODO(1): decode BGnCNT. Priority bits 0-1, char base bits 2-3 in 16 KiB
// units, mosaic bit 6, 8bpp bit 7, screen base bits 8-12 in 2 KiB units,
// size bits 14-15 selects 32/64-tile map dimensions.
inline TextBgConfig decode_text_bg_config(u16 cnt) {
    (void)cnt;
    return {};  // wrong on purpose
}
//@LABS-END

// Convenience wrapper so tests read like hardware usage (BGnCNT at IO+8+2n).
inline TextBgConfig get_text_bg_config(const PpuMemory& m, int bg) {
    return decode_text_bg_config(m.rd16(PpuMemory::kIoBase + 8 + 2 * u32(bg)));
}

// Screen entry: tile number bits 0-9, hflip bit 10, vflip bit 11,
// palette bank bits 12-15.
struct ScreenEntry {
    u16 raw = 0;
    int tile() const { return raw & 0x3FF; }
    bool hflip() const { return (raw >> 10) & 1; }
    bool vflip() const { return (raw >> 11) & 1; }
    int bank() const { return (raw >> 12) & 0xF; }
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline ScreenEntry decode_screen_entry(u16 raw) {
    ScreenEntry e;
    e.raw = raw;
    return e;
}
//@LABS-STUB
// TODO(2): decode a screen entry (tile bits 0-9, flips bits 10/11, palette
// bank bits 12-15).
inline ScreenEntry decode_screen_entry(u16 raw) {
    (void)raw;
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// One pixel of an 8x8 tile at byte address `tile_base`.
// 4bpp: 32 bytes/tile, row-major, low nibble = even x; color index is
// bank*16 + nibble; nibble 0 is transparent (-1).
// 8bpp: 64 bytes/tile, one byte per pixel; 0 is transparent.
inline int tile_pixel(const PpuMemory& m, u32 tile_base, int tx, int ty,
                      bool bpp8, int bank) {
    if (unsigned(tx) > 7u || unsigned(ty) > 7u) return -1;
    if (!bpp8) {
        u8 byte = m.vram[(tile_base + u32(ty) * 4 + u32(tx >> 1)) %
                         kVramSize];
        int idx = (tx & 1) ? (byte >> 4) : (byte & 0xF);
        return idx == 0 ? -1 : bank * 16 + idx;
    }
    u8 byte = m.vram[(tile_base + u32(ty) * 8 + u32(tx)) % kVramSize];
    return byte == 0 ? -1 : int(byte);
}
//@LABS-STUB
// TODO(3): fetch one pixel from an 8x8 tile. 4bpp: 32 bytes/tile, row-major,
// low nibble = even x; color = bank*16 + nibble, nibble 0 transparent (-1).
// 8bpp: 64 bytes/tile, one byte per pixel, byte 0 transparent (-1).
inline int tile_pixel(const PpuMemory& m, u32 tile_base, int tx, int ty,
                      bool bpp8, int bank) {
    (void)m;
    (void)tile_base;
    (void)tx;
    (void)ty;
    (void)bpp8;
    (void)bank;
    return -2;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Resolve one background pixel to a palette index, or -1 when transparent.
// Scroll registers wrap at 512 (masked to 9 bits); the map repeats modulo
// its pixel size; maps larger than 256x256 are stored as quadrants of
// 32x32-entry blocks.
inline int text_bg_pixel(const PpuMemory& m, const TextBgConfig& cfg,
                         int x, int y) {
    u32 hofs = m.rd16(PpuMemory::kIoBase + 0x10) & 0x1FF;
    u32 vofs = m.rd16(PpuMemory::kIoBase + 0x12) & 0x1FF;
    int px = (x + int(hofs)) % (cfg.map_w_tiles * 8);
    int py = (y + int(vofs)) % (cfg.map_h_tiles * 8);
    int tx = px / 8, ty = py / 8;
    u32 block = u32(tx / 32) + u32(ty / 32) * 32;
    u16 raw = m.rd16(PpuMemory::kVramBase + u32(cfg.screen_base) +
                     block * 0x800 + u32(ty % 32) * 64 + u32(tx % 32) * 2);
    ScreenEntry e = decode_screen_entry(raw);
    int lx = e.hflip() ? 7 - px % 8 : px % 8;
    int ly = e.vflip() ? 7 - py % 8 : py % 8;
    u32 tile_base =
        u32(cfg.char_base) + u32(e.tile()) * (cfg.bpp8 ? 64u : 32u);
    return tile_pixel(m, tile_base, lx, ly, cfg.bpp8, e.bank());
}
//@LABS-STUB
// TODO(4): resolve one text-BG pixel to a palette index (-1 transparent).
// Steps: apply 9-bit-masked scroll offsets, wrap by map pixel size, look up
// the screen entry (quadrant blocks of 32x32 entries for big maps), honor
// h/vflip, then fetch the tile pixel.
inline int text_bg_pixel(const PpuMemory& m, const TextBgConfig& cfg,
                         int x, int y) {
    (void)m;
    (void)cfg;
    (void)x;
    (void)y;
    return -2;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Compose one scanline over enabled text backgrounds: per pixel the visible
// layer is the non-transparent background with the lowest priority value;
// ties break toward the lower BG number (input order). Output holds palette
// indices, -1 = show backdrop.
inline void compose_text_scanline(const PpuMemory& m,
                                  const std::vector<TextBgConfig>& bgs, int y,
                                  int out_indices[kScreenW]) {
    for (int x = 0; x < kScreenW; ++x) {
        int best_idx = -1;
        int best_prio = 4;
        for (size_t i = 0; i < bgs.size(); ++i) {
            const TextBgConfig& cfg = bgs[i];
            if (i > 0 && cfg.priority >= best_prio)
                continue;  // cannot win: tie keeps the earlier BG
            int idx = text_bg_pixel(m, cfg, x, y);
            if (idx >= 0 && cfg.priority < best_prio) {
                best_idx = idx;
                best_prio = cfg.priority;
            }
        }
        out_indices[x] = best_idx;
    }
}
//@LABS-STUB
// TODO(5): pick, per pixel, the non-transparent background with the lowest
// priority value; equal priorities go to the lower BG number (input order).
// Write the winning palette index or -1 for backdrop.
inline void compose_text_scanline(const PpuMemory& m,
                                  const std::vector<TextBgConfig>& bgs, int y,
                                  int out_indices[kScreenW]) {
    (void)m;
    (void)bgs;
    (void)y;
    for (int x = 0; x < kScreenW; ++x) out_indices[x] = -2;  // wrong on purpose
}
//@LABS-END

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
