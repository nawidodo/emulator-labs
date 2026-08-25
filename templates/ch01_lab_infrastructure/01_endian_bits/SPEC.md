# SPEC — 01_endian_bits

## Functions

```cpp
uint16_t read_le16(const uint8_t* p);   // little-endian: low byte at p[0]
uint16_t read_be16(const uint8_t* p);   // big-endian:    high byte at p[0]
uint32_t read_le32(const uint8_t* p);   // little-endian, four bytes
uint32_t bits(uint32_t value, unsigned start, unsigned count);
```

`bits(value, start, count)` returns `(value >> start) & ((1 << count) - 1)`
with the special case `count == 32 -> value >> start` (a 32-bit shift by 32
would be undefined). Preconditions: `count >= 1`, `start + count <= 32`.

## Why explicit widening

`p[1] << 8` promotes through `int`; that happens to work for 8-bit loads,
but the habit breaks the moment a signed char or a 32-assembled byte lands
in bit 31. Cast every byte to the target width *before* shifting. The
reference solution does this everywhere; keep it in yours.

## Real-world anchors

- CHIP-8 opcodes are big-endian 16-bit words (`read_be16`).
- Game Boy cartridge header checksums and NES iNES headers are
  little-endian (`read_le16/read_le32`).
- CPU opcode fields (register numbers, addressing modes) are extracted with
  exactly the `bits()` idiom.

## Acceptance

All tests in `main.cpp` pass (`ctest -R ch01_01_endian_bits`). In the
generated skeleton every function returns 0 and the suite runs RED; fill in
the four `TODO(n)` blocks one checkpoint at a time.
