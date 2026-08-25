# Coding test — the MATH-X extension (unseen specification)

You have never seen these opcodes. Implement them in
`99_coding_test/chip8.hpp` (stubs marked TODO(1)..TODO(5)) from THIS spec
alone, exactly and completely. Hidden grading executes them.

All five live in the same machine as the base CHIP-8 CPU. Unspecified bits
of behavior follow the base machine conventions (12-bit wrap, uint8 registers).

## Opcode specification

### 8XY8 — AVG
```text
sum = VX + VY          (computed in 16 bits, no truncation before divide)
VX  = sum / 2          (integer division)
VF  = sum & 1          (the low bit lost by the division)
```
Example: VX=07, VY=04 -> VX=05, VF=01. VX=C8, VY=32 -> VX=7D, VF=00.

### 8XY9 — MIN
```text
if VY < VX: VX = VY; VF = 1
else:       VF = 0      (both operands untouched)
```
Equality replaces nothing: VX=VY=5 leaves both at 5 with VF=0.

### 8XYA — MAX
```text
if VY > VX: VX = VY; VF = 1
else:       VF = 0
```

### 8XYB — MULLO
```text
prod = VX * VY          (16-bit product)
VX   = prod & 0xFF
VF   = prod >> 8        (high byte; 0 when the product fits in 8 bits)
```
Boundary: FF*FF = FE01 -> VX=01, VF=FE.

### FXY2 — XSUM
```text
acc = XOR of mem[I], mem[I+1], ..., mem[I+VY]     (inclusive range)
VX  = acc
VF  = 0                (always cleared)
I   = unchanged
```
Addresses wrap at 0x1000 like all memory access. VY=00 folds the single
byte mem[I].

## What is graded

Hidden cases execute unit checks per opcode (boundaries above included) and
a chained program using several extensions together. Unassigned codes
(8XY8-B are assigned; 8XYC/8XYD stay illegal; FX** other than FXY2 stay
illegal) must still halt the machine.
