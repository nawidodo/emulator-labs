#pragma once
#include <cstdint>

namespace gba {

// GBA bus regions (see LECTURE.md memory map). OpenBus covers unmapped /
// write-only reads answered from the last driven data value.
enum class Region : uint8_t {
    Bios, Ewram, Iwram, Io, Palette, Vram, Oam,
    RomWs0, RomWs1, RomWs2, Sram, OpenBus,
};

// Non-sequential / sequential access cost per region, in cycles.
// Everything internal runs at 1/1; waitstated chips are slower.
struct AccessCost { unsigned n, s; };

constexpr AccessCost kInternal{1, 1};
inline AccessCost cost_of(Region r) {
    switch (r) {
    case Region::Ewram:   return {3, 2};
    case Region::RomWs0:  return {4, 2};
    case Region::RomWs1:  return {3, 2};
    case Region::RomWs2:  return {5, 2};
    case Region::Sram:    return {5, 2};
    default:              return kInternal;
    }
}

}  // namespace gba
