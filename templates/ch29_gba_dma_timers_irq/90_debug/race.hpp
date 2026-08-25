#pragma once
// Debugging exercise: two seeded timing defects that corrupt event ordering.
//
// The STUB sides below contain SEEDED BUGS; the SOLUTION sides match real
// hardware. See DEBUGGING.md for observed symptoms.
#pragma once
#include <cstdint>

namespace gba {

using u32 = uint32_t;
using u16 = uint16_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr u32 kCyclesPerLine = 1232;
constexpr u32 kHblankStart = 960;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Guest cycle at which HBlank-triggered DMA fires for a given scanline.
// HBlank begins 960 cycles INTO each line.
inline u64 hblank_dma_cycle(u32 line) {
    return u64(line) * kCyclesPerLine + kHblankStart;
}
//@LABS-STUB
// TODO(1): this schedule lands one scanline away from where hardware puts
// it — games see sprite/palette updates applied to the wrong line.
inline u64 hblank_dma_cycle(u32 line) {
    return (u64(line) + 1) * kCyclesPerLine + kHblankStart;  // seeded bug
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Ticks a reloaded timer needs until its next overflow: from the reload
// value it counts up through 0xFFFF and wraps, so exactly
// `0x10000 - reload` ticks.
inline u32 timer_period_ticks(u16 reload) {
    return u32(0x10000u) - reload;
}

// The counter value right after an overflow.
inline u16 timer_post_overflow_counter(u16 reload) {
    return reload;
}
//@LABS-STUB
// TODO(2): both functions are off by one tick relative to hardware:
// compute the period from the reload value and the post-overflow counter.
inline u32 timer_period_ticks(u16 reload) {
    return u32(0x10000u) - reload - 1u;  // seeded bug
}
inline u16 timer_post_overflow_counter(u16 reload) {
    return u16(reload + 1u);             // seeded bug
}
//@LABS-END

}  // namespace gba
