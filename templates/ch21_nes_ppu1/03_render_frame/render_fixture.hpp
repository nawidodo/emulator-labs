#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

#include "../01_ppu_bus/ppu_bus.hpp"
#include "../02_bg_pipeline/bg_pipeline.hpp"
#include "fixture.hpp"

// Chapter 21 frame renderer: turns a crafted NESF snapshot into a full
// 256x240 RGBA8 frame. Chapter-21 simplifications, documented:
//   * no scrolling — the nametable window is the $2000 base selected by
//     PPUCTRL bits 0-1 (loopy v/t arrive in Chapter 22);
//   * no sprites — OAM is carried in the fixture for forward compatibility.
//
// The logical nametable image is resolved through the real PpuBus so
// mirroring behavior from exercise 01 is exercised end-to-end.
namespace nes21render {

inline void render_snapshot_frame(std::array<uint8_t, 256 * 240 * 4>& out,
                                  const nes21fix::Snapshot& snap) {
    nes21bus::PpuBus bus(static_cast<nes21bus::Mirroring>(snap.mirroring));
    bus.chr = snap.chr;
    std::copy(snap.nt.begin(), snap.nt.end(), bus.vram.begin());
    bus.palette = snap.pal;

    // Resolve the PPUCTRL-selected logical nametable through the
    // mirroring hardware: base = $2000 + (ctrl & 3) * $400.
    std::array<uint8_t, 0x0400> nt_window{};
    uint16_t nt_base = uint16_t(0x2000 + (snap.ctrl & 0x03) * 0x0400);
    for (int i = 0; i < 0x0400; ++i)
        nt_window[i] = bus.read(uint16_t(nt_base + i));

    nes21bg::FrameInputs in{bus.chr.data(), nt_window.data(),
                            bus.palette.data(), snap.ctrl};
    nes21bg::render_frame(out, in);
}

}  // namespace nes21render
