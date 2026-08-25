#pragma once
// Quirk profiles: the historical CHIP-8 family tree, made explicit
// (curriculum ch6 "quirks").
//
//   COSMAC_VIP  the 1977 RCA original most software was written for
//   CHIP48      the 1990s CHIP-48 fork; introduced the BXNN jump bug and
//               made FX55/FX65 advance I
//   MODERN      what most interpreters do today
//
// A program assembled for one variant silently misbehaves on another —
// which is why profiles must be a first-class configuration knob.

#include <optional>
#include <string>

namespace ch06 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
struct Chip8Quirks {
    // FX1E/FX55/FX65 clear VF first (modern) or leave VF alone (VIP/CHIP48).
    bool vf_reset = true;
    // 8XY6/8XYE shift VY into VX (VIP/CHIP48) or VX in place (modern).
    bool shift_uses_vy = false;
    // FX55/FX65 leave I untouched (VIP) or advance it by X+1 (CHIP48/modern).
    bool load_store_leaves_i = false;
    // DXYN wraps at screen edges (VIP) or clips (CHIP48/modern).
    bool wrapping = false;
    // BXNN adds register X named by the opcode (CHIP48 bug) or V0.
    bool jump_bnnn_x = false;
};
inline constexpr Chip8Quirks kModernQuirks{};
//@LABS-STUB
// TODO(1): define the five quirk flags exactly with these names:
//   vf_reset, shift_uses_vy, load_store_leaves_i, wrapping, jump_bnnn_x
// (bool each; wrong defaults on purpose so the tests stay RED).
struct Chip8Quirks {
    bool vf_reset = false;
    bool shift_uses_vy = false;
    bool load_store_leaves_i = false;
    bool wrapping = false;
    bool jump_bnnn_x = false;
};
inline constexpr Chip8Quirks kModernQuirks{};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Returns the preset for a profile name, or nullopt if unknown.
inline std::optional<Chip8Quirks> profile_by_name(const std::string& name) {
    if (name == "MODERN") return Chip8Quirks{true, false, false, false, false};
    if (name == "COSMAC_VIP")
        return Chip8Quirks{/*vf_reset*/ false, /*shift_uses_vy*/ true,
                           /*load_store_leaves_i*/ true, /*wrapping*/ true,
                           /*jump_bnnn_x*/ false};
    if (name == "CHIP48")
        return Chip8Quirks{/*vf_reset*/ false, /*shift_uses_vy*/ true,
                           /*load_store_leaves_i*/ false, /*wrapping*/ false,
                           /*jump_bnnn_x*/ true};
    return std::nullopt;
}
//@LABS-STUB
// TODO(2): return the preset for "MODERN", "COSMAC_VIP" and "CHIP48".
//   COSMAC_VIP: shift_uses_vy, load_store_leaves_i, wrapping set;
//               vf_reset and jump_bnnn_x clear.
//   CHIP48:     shift_uses_vy and jump_bnnn_x set; everything else clear.
// Unknown names -> nullopt.
inline std::optional<Chip8Quirks> profile_by_name(const std::string& name) {
    (void)name;
    return std::nullopt;
}
//@LABS-END

}  // namespace ch06
