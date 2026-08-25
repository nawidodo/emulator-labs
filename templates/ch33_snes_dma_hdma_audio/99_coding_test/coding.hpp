#pragma once
// Coding test entry (chapter 33): "Transfer Mode X", an UNSEEN DMA pattern
// variant specified in SPEC.md (chapter root) and CODING_TEST.md.
//
// Mode X in one glance (full normative text + worked example in SPEC.md):
//   * units per transfer : 4
//   * B-bus offsets      : +0, +1, +2, +1   (the +2 unit is the twist)
//   * A-bus step         : ALWAYS +1 per byte, regardless of control
//                          bits 4-3
#include <cstdint>
#include <span>
#include <vector>

namespace snesdma::variant {

struct Channel {
    uint8_t control = 0;      // $43x0; only bit 2-0 style fields matter here,
                              // and even those are ignored by mode X itself
    uint8_t b_reg = 0;        // $43x1 BBADx
    uint16_t a_addr = 0;      // $43x2-$43x3
    uint8_t a_bank = 0;       // $43x4
    uint16_t unit_count = 0;  // $43x5 bytes to move
};

struct TransferStep {
    uint16_t b_addr = 0;
    uint32_t a_addr = 0;
};

inline constexpr int kBbusBase = 0x2100;
inline constexpr int kUnitsPerTransferX = 4;

// B-bus register OFFSET of unit `unit` (always < 4) within one transfer.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline constexpr uint8_t kUnitBOffsetX[4] = {0, 1, 2, 1};

inline uint8_t unit_b_offset_x(int unit) {
    return kUnitBOffsetX[unit];
}
//@LABS-STUB
// TODO(1): return the B-bus offset for unit `unit` of transfer mode X.
// The SPEC.md worked example lists all four offsets explicitly.
inline uint8_t unit_b_offset_x(int /*unit*/) {
    return 0;  // wrong on purpose
}
//@LABS-END

// Runs a full mode-X transfer byte by byte. Note the deliberate hardware
// twist from SPEC.md: mode X IGNORES control bits 4-3 and always walks the
// A address upward.
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline std::vector<TransferStep> run_mode_x(const Channel& ch,
                                            std::span<const uint8_t> a_bus) {
    std::vector<TransferStep> log;
    log.reserve(ch.unit_count);
    uint32_t a = (uint32_t(ch.a_bank) << 16) | ch.a_addr;
    for (uint16_t i = 0; i < ch.unit_count; ++i) {
        if (a >= a_bus.size()) break;
        log.push_back({uint16_t(kBbusBase + ch.b_reg +
                                unit_b_offset_x(i % kUnitsPerTransferX)),
                       a});
        ++a;  // mode X always increments, no exceptions
    }
    return log;
}
//@LABS-STUB
// TODO(2): perform the mode-X transfer like run_channel in exercise 01,
// but with the mode-X offset pattern and the FORCED +1 A step.
inline std::vector<TransferStep> run_mode_x(const Channel& /*ch*/,
                                            std::span<const uint8_t> /*a_bus*/) {
    return {};  // wrong on purpose: no transfers performed
}
//@LABS-END

}  // namespace snesdma::variant
