// header.hpp — Game Boy cartridge header decoding ($0100-$014F).
//
// Every GB ROM carries a fixed header inside bank 0. The CPU never runs
// this data; the boot ROM reads it to decide whether a cartridge is even
// present (logo check) and the emulator reads it to pick a mapper, size
// its RAM, and label the game. All offsets below are absolute ROM
// addresses:
//
//   $0134-$0143  title (16 raw bytes, upper-case ASCII on real carts)
//   $0147        cartridge type (mapper + peripherals byte)
//   $0148        ROM size code (0 -> 32 KiB, code N -> 32 KiB << N)
//   $0149        RAM size code
//   $014D        header checksum
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cart {

inline constexpr uint16_t kTitleAddr = 0x0134;
inline constexpr size_t kTitleLen = 16;
inline constexpr uint16_t kTypeAddr = 0x0147;
inline constexpr uint16_t kRomSizeAddr = 0x0148;
inline constexpr uint16_t kRamSizeAddr = 0x0149;
inline constexpr uint16_t kChecksumAddr = 0x014D;

// Raw 16-byte title at $0134, verbatim: no trimming, no case folding.
// Games pad with $00 or spaces and we keep both so byte-exact comparisons
// against a fixture stay possible.
inline std::string title(const uint8_t* rom) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return std::string(reinterpret_cast<const char*>(rom + kTitleAddr),
                       kTitleLen);
//@LABS-STUB
    // TODO(1): copy the 16 raw bytes at $0134 into a std::string.
    (void)rom;
    return "";
//@LABS-END
}

// True when the cartridge type byte includes a battery-backed SRAM chip.
// Battery types (Pan Docs "cartridge types"): $03, $06, $09, $0D, $0F,
// $10, $13, $1B, $1E.
inline bool hasBattery(uint8_t type) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    switch (type) {
        case 0x03: case 0x06: case 0x09: case 0x0D: case 0x0F:
        case 0x10: case 0x13: case 0x1B: case 0x1E:
            return true;
        default:
            return false;
    }
//@LABS-STUB
    // TODO(2): report whether this type byte includes a battery.
    (void)type;
    return false;
//@LABS-END
}

// Human-readable mapper/peripheral name for the type byte, e.g.
// "MBC1", "MBC1+RAM+BATTERY", "MBC3+TIMER+BATTERY", "MBC5+RUMBLE".
// Unknown bytes return "UNKNOWN".
inline const char* controllerName(uint8_t type) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    switch (type) {
        case 0x00: return "ROM_ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x05: return "MBC2";
        case 0x06: return "MBC2+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        default: return "UNKNOWN";
    }
//@LABS-STUB
    // TODO(3): map the type byte to its controller name string.
    (void)type;
    return "UNKNOWN";
//@LABS-END
}

// ROM size in bytes for the $0148 code: codes $00-$08 are 32 KiB << code.
// Oddball codes $52/$53/$54 encode the truncated 1 MiB-class masks used
// on a handful of late MBC1/MBC3 carts. This lab adopts the canonical
// sizes 1 MiB / 1152 KiB / 1280 KiB and stays consistent with the
// fixture generator in tests/public/ch16_gameboy_cartridges_mbc/tools/.
inline size_t romSizeBytes(uint8_t code) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    switch (code) {
        case 0x52: return 1048576;   // 1 MiB
        case 0x53: return 1179648;   // 1152 KiB
        case 0x54: return 1310720;   // 1280 KiB
        default:
            if (code > 0x08) return 0;  // undocumented code: no sane size
            return size_t{32768} << code;
    }
//@LABS-STUB
    // TODO(4): expand the $0148 code into a ROM size in bytes.
    (void)code;
    return 0;
//@LABS-END
}

// RAM size in bytes for the $0149 code.
inline size_t ramSizeBytes(uint8_t code) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
    switch (code) {
        case 0x00: return 0;
        case 0x01: return 2048;      // 2 KiB (quirky 900-byte carts aside)
        case 0x02: return 8192;      // 8 KiB = 1 bank
        case 0x03: return 32768;     // 32 KiB = 4 banks
        case 0x04: return 131072;    // 128 KiB = 16 banks
        case 0x05: return 65536;     // 64 KiB = 8 banks
        default: return 0;
    }
//@LABS-STUB
    // TODO(5): expand the $0149 code into a RAM size in bytes.
    (void)code;
    return 0;
//@LABS-END
}

// Header checksum over $0134-$014D. The boot ROM requires
//   sum($0134..$014C) + byte($014D) + 25 == 0 (mod 256),
// i.e. the stored byte is (-(sum + 25)) & 0xFF. Same convention as the
// fixture generator; corrupting ANY header byte breaks the equality,
// which is exactly why Nintendo chose a range that covers title, type,
// and both size codes.
inline bool headerChecksumValid(const uint8_t* rom) {
//@LABS-BEGIN 6
//@LABS-SOLUTION
    unsigned sum = 25;
    for (uint16_t a = kTitleAddr; a <= kChecksumAddr; ++a) sum += rom[a];
    return (sum & 0xFF) == 0;
//@LABS-STUB
    // TODO(6): verify sum($0134..$014C) + $014D + 25 == 0 mod 256.
    (void)rom;
    return false;
//@LABS-END
}

}  // namespace cart
