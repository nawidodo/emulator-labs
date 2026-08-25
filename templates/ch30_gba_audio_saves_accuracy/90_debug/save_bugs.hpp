#pragma once
// Debugging exercise: two seeded save-media defects.
//
// The STUB sides contain SEEDED BUGS; the SOLUTION sides are correct. See
// DEBUGGING.md. Flash model duplicated (simplified) from 03_save_flash.
#pragma once
#include <cstdint>
#include <cstring>

namespace gba {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Program one byte into a flash page: flash cells can only be flipped from
// 1 to 0 until an erase restores them, so the result is old AND new.
inline void flash_program_byte(u8& cell, u8 value) { cell &= value; }

// Address a byte inside the cartridge save window: 128 KiB devices use
// bank << 16 | (addr & 0x1FFFF); 64 KiB devices ignore the bank and mask
// to 16 bits.
inline u32 flash_byte_offset(bool is128k, u8 bank, u32 addr) {
    u32 off = (u32(bank) << 16) | (addr & (is128k ? 0x1FFFFu : 0xFFFFu));
    if (!is128k) off &= 0xFFFF;
    return off;
}
//@LABS-STUB
// TODO(1): both functions carry one seeded defect each: the program
// operation must never SET bits (only clear), and 64 KiB devices must not
// let bank/address bits alias outside their 64 KiB window.
inline void flash_program_byte(u8& cell, u8 value) { cell = value; }
inline u32 flash_byte_offset(bool is128k, u8 bank, u32 addr) {
    return (u32(bank) << 16) | (addr & 0x1FFFFu);
}
//@LABS-END

}  // namespace gba
