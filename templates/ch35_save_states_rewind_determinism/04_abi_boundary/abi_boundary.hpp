// abi_boundary.hpp — host/core boundary for the CHIP-8 core, done the way
// a stable C ABI demands (foundations track Phase 6, chapters 77-79):
//
//   * opaque handle: callers never touch the machine struct directly
//   * versioned config struct with struct_size + abi_version fields
//   * ROM bytes are PROVIDED BY THE HOST — the core performs no file IO
//   * fixed-width public types only; layout is part of the contract
//
// The types are plain standard-layout so the same header could compile as
// C17 with `extern "C"` stripped; static_asserts pin the layout.
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace chip8abi {

// --- public fixed-width result codes ------------------------------------

enum : int {
    CH8_OK = 0,
    CH8_ERR_VERSION = 1,
    CH8_ERR_SIZE = 2,
    CH8_ERR_NO_ROM = 3,
    CH8_ERR_ROM_TOO_BIG = 4,
};

constexpr uint32_t kAbiVersion = 1;
constexpr uint16_t kRomMaxBytes = 3584;  // 0x200..0xFFF

// --- versioned configuration struct (caller allocates) ------------------

struct Ch8Config {
    uint32_t struct_size;    // sizeof(Ch8Config) — ABI safety valve
    uint32_t abi_version;    // kAbiVersion
    uint32_t cycles_per_frame;
    uint32_t flags;          // bit0: wrap sprites; bit1: vf-reset quirk
};

// Layout is contract: any change must bump kAbiVersion.
static_assert(sizeof(Ch8Config) == 16, "Ch8Config layout is ABI");
static_assert(std::is_standard_layout_v<Ch8Config>);

// --- opaque machine handle ----------------------------------------------

struct Ch8Machine;                       // opaque
Ch8Machine* ch8_create(const Ch8Config* cfg, int* out_err);
void ch8_destroy(Ch8Machine* m);

// Host-provided ROM bytes (max 3584). Copied into the core; no filesystem.
int ch8_load_rom(Ch8Machine* m, const uint8_t* bytes, uint16_t size);

// Deterministic step: run exactly one frame's worth of cycles and tick the
// 60 Hz timers once. Keypad bit i set = key i pressed (host supplies).
int ch8_run_frame(Ch8Machine* m, uint16_t keypad_mask);

// Framebuffer readout: 64x32 one-byte-per-pixel shades 0/1.
int ch8_read_frame(const Ch8Machine* m, uint8_t* out_2048);
int ch8_read_delay_timer(const Ch8Machine* m, uint8_t* out);

}  // namespace chip8abi
