#pragma once
#include <cstdint>
#include "bus.hpp"

namespace psx::r3000a {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint32_t do_lw(Bus& bus, uint32_t addr) { return bus.read32(addr); }
inline void do_sw(Bus& bus, uint32_t addr, uint32_t val) { bus.write32(addr, val); }
//@LABS-STUB
// TODO(1): word load/store. lw reads a 32-bit little-endian word at the
// ALIGNED address addr; sw writes one. Bus::read32/write32 already do the
// byte work — this block is about wiring the executor to them.
inline uint32_t do_lw(Bus& bus, uint32_t addr) {
    (void)bus;
    (void)addr;
    return 0;  // TODO(1)
}
inline void do_sw(Bus&, uint32_t, uint32_t) {
    // TODO(1)
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// lb sign-extends the loaded byte; lbu zero-extends. This distinction is why
// both encodings exist — C's `char` signedness made compilers emit lb/lbu
// constantly in PS1 code.
inline uint32_t do_load_byte(Bus& bus, uint32_t addr, bool sign_extend) {
    const uint32_t v = bus.read8(addr);
    if (!sign_extend || !(v & 0x80u)) return v;
    return v | 0xFFFFFF00u;
}
inline void do_sb(Bus& bus, uint32_t addr, uint32_t val) { bus.write8(addr, uint8_t(val)); }
//@LABS-STUB
// TODO(2): byte load/store. do_load_byte loads one byte; when sign_extend is
// true and bit 7 is set, replicate it into bits 8..31 (0xFFFFFF00 mask).
inline uint32_t do_load_byte(Bus& bus, uint32_t addr, bool sign_extend) {
    (void)bus;
    (void)addr;
    (void)sign_extend;
    return 0;  // TODO(2)
}
inline void do_sb(Bus&, uint32_t, uint32_t) {
    // TODO(2)
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint32_t do_load_half(Bus& bus, uint32_t addr, bool sign_extend) {
    const uint32_t v = bus.read16(addr);
    if (!sign_extend || !(v & 0x8000u)) return v;
    return v | 0xFFFF0000u;
}
inline void do_sh(Bus& bus, uint32_t addr, uint32_t val) { bus.write16(addr, uint16_t(val)); }
//@LABS-STUB
// TODO(3): halfword load/store. Same pattern as bytes but 16-bit, sign bit
// is bit 15, extension mask 0xFFFF0000.
inline uint32_t do_load_half(Bus& bus, uint32_t addr, bool sign_extend) {
    (void)bus;
    (void)addr;
    (void)sign_extend;
    return 0;  // TODO(3)
}
inline void do_sh(Bus&, uint32_t, uint32_t) {
    // TODO(3)
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// The unaligned LOAD pair. With b = addr & 3:
//   LWR keeps rt's high b bytes, fills the low (4-b) from memory at addr.
//   LWL keeps rt's low (3-b) bytes, fills the high (b+1) ending at addr.
// Together they load any unaligned word in two instructions without ever
// touching a misaligned bus cycle.
inline uint32_t do_lwr(Bus& bus, uint32_t addr, uint32_t old_rt) {
    const uint32_t b = addr & 3u;
    const uint32_t word = bus.read32(addr & ~3u);
    const uint32_t keep_high = b == 0 ? 0u : (0xFFFFFFFFu << ((4u - b) * 8u));
    return (old_rt & keep_high) | ((word >> (b * 8u)) & ~keep_high);
}
inline uint32_t do_lwl(Bus& bus, uint32_t addr, uint32_t old_rt) {
    const uint32_t b = addr & 3u;
    const uint32_t word = bus.read32(addr & ~3u);
    const uint32_t keep_low = b == 3 ? 0u : (0xFFFFFFFFu >> ((b + 1u) * 8u));
    return (old_rt & keep_low) | ((word << ((3u - b) * 8u)) & ~keep_low);
}
//@LABS-STUB
// TODO(4): implement the LWR/LWL unaligned-load pair (little-endian rules).
// See LECTURE.md "LWL/LWR semantics": with b = addr & 3,
//   LWR: rt = (old_rt & keep_high(b)) | (aligned_word >> (b*8))
//   LWL: rt = (old_rt & keep_low(3-b)) | (aligned_word << ((3-b)*8))
// where aligned_word = read32(addr & ~3). The lwr+lwl sequence must assemble
// any unaligned word; tests pin every b combination.
inline uint32_t do_lwr(Bus& bus, uint32_t addr, uint32_t old_rt) {
    (void)bus;
    (void)addr;
    (void)old_rt;
    return 0;  // TODO(4)
}
inline uint32_t do_lwl(Bus& bus, uint32_t addr, uint32_t old_rt) {
    (void)bus;
    (void)addr;
    (void)old_rt;
    return 0;  // TODO(4)
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Store mirror of the load pair. With b = addr & 3 (little-endian, byte i of
// the aligned word = bits [8i+7 : 8i]):
//   SWR writes the LOW (4-b) bytes of val to word bytes b..3  -> bits >= 8b.
//   SWL writes the HIGH (b+1) bytes of val to word bytes 0..b -> bits < 8(b+1).
inline void do_swr(Bus& bus, uint32_t addr, uint32_t val) {
    const uint32_t b = addr & 3u;
    const uint32_t aligned = addr & ~3u;
    const uint32_t wmask = b == 0 ? 0xFFFFFFFFu : (0xFFFFFFFFu << (b * 8u));
    bus.write32(aligned, (bus.read32(aligned) & ~wmask) | ((val << (b * 8u)) & wmask));
}
inline void do_swl(Bus& bus, uint32_t addr, uint32_t val) {
    const uint32_t b = addr & 3u;
    const uint32_t aligned = addr & ~3u;
    const uint32_t wmask = b == 3 ? 0xFFFFFFFFu : (0xFFFFFFFFu >> ((3u - b) * 8u));
    const uint32_t bytes = val >> ((3u - b) * 8u);  // high (b+1) bytes of val
    bus.write32(aligned, (bus.read32(aligned) & ~wmask) | (bytes & wmask));
}
//@LABS-STUB
// TODO(5): implement the SWR/SWL unaligned-store pair. Mirror of block 4:
//   SWR: mem[addr..] gets the LOW (4-b) bytes of val, other bytes preserved.
//   SWL: mem[..addr] gets the HIGH (b+1) bytes of val, other bytes preserved.
// A swr+swl sequence must store an unaligned word exactly; tests cover all
// four alignments by round-tripping through the load pair.
inline void do_swr(Bus&, uint32_t, uint32_t) {
    // TODO(5)
}
inline void do_swl(Bus&, uint32_t, uint32_t) {
    // TODO(5)
}
//@LABS-END

}  // namespace psx::r3000a
