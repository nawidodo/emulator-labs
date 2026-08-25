# SPEC — ch09_space_invaders_machine

This chapter composes the i8080 CPU from chapters 7–8 with a machine
layer built in this chapter. Composition decisions and the documented
hardware model live here.

## CPU composition

The exercises are self-contained: each directory that runs code carries a
compact copy of the 8080 core (`flags.hpp`, `alu.hpp`, `cpu.hpp`,
`cpu.cpp`). The core is the chapter 8 control-flow core carried forward
verbatim and extended with:

- `Bus::in(port)` / `Bus::out(port, val)` virtual hooks (neutral
  defaults: reads float low, writes vanish) so a bare `FlatBus` still
  works,
- the IN (DB) / OUT (D3) opcodes, 10 T-states each,
- the register-pair group (INX/DCX/DAD), STAX/LDAX, LHLD/SHLD, rotates,
  CMA/STC/CMC/DAA and XTHL — everything fixture programs use.

Trace format is unchanged from chapters 7–8:
`pc= op= af= bc= de= hl= sp= cyc=` (uppercase hex values, line printed
BEFORE executing each instruction). Final-state lines keep the ch08
shape `AF=.. BC=.. .. PC=.. cyc=..`.

## Documented hardware model

- **Clock:** fixed 1920 kHz → exactly **32000 T-states per frame** at
  60 Hz. Historical boards ran ~2 MHz with sloppy vertical timing;
  determinism beats authenticity here.
- **Memory map:** ROM `0000-1FFF` (four linear 2 KiB banks, writes
  ignored, unprogrammed tail reads 0xFF), RAM `2000-23FF`, VRAM
  `2400-3FFF`. No mirroring anywhere; unmapped reads float low; unmapped
  writes drop.
- **VRAM:** column-major — byte `(col*32 + y/8)`, bit `y%8` is pixel
  `(col, y)`. Rendering to upright 224×256 RGBA8888 happens in this
  decode. Palette: white on black, alpha opaque.
- **Interrupts:** dual one-shot vblank timers alternating per frame
  period — even frames jam RST 08 (0xCF), odd frames jam RST 10 (0xD7).
  One interrupt per frame boundary; a masked CPU loses the pulse (edge
  one-shots, not level latches).
- **Shift register:** 16-bit TTL part. OUT 4 shifts old high half to low
  half and inserts the written byte high (two writes fill LOW then HIGH);
  OUT 2 bits 0-2 latch a 3-bit amount that wraps mod 8; IN 3 returns bits
  `[amount..amount+7]`.
- **Sound ports 3/5:** documented stubs appending `(cycle, port, value)`
  events to an in-memory recorder. **Watchdog port 6:** records the kick;
  expiry is queryable but never resets the board mid-test.

### Port map (machine A)

| Port | Dir | Function |
|---|---|---|
| IN 0 | R | coin/service latch |
| IN 1 | R | bit0 left, bit1 right, bit2 fire, bit3 1P start, bit4 2P start |
| IN 2 | R | dip-switch latch |
| IN 3 | R | shift-register result |
| OUT 2 | W | bits 0-2 = shift amount |
| OUT 3 | W | sound event |
| OUT 4 | W | shift-register data write |
| OUT 5 | W | sound event |
| OUT 6 | W | watchdog kick (+event) |

## Scripted input protocol

One line per frame period: three hex bytes `P0 P1 P2` (missing trailing
bytes are an error; blank lines and `#` comments skipped). The runner
advances one line at every frame boundary.

## Machine B (coding test)

A fictional second board specified entirely by `MachineSpec` in
`99_coding_test/machine_b.hpp` and CODING_TEST.md: 16 KiB ROM / 1 KiB RAM
at 4000 / 7 KiB VRAM at 4400, 2160 kHz (36000 cycles/frame), vectors
RST→0x18 (even) / RST→0x30 (odd), two shift registers sharing OUT 2 for
amounts, sound on OUT 4/5, watchdog on OUT 0.

## Deviation note

Real Space Invaders boards read the shifter on IN 3 and drive sound from
OUT 3/5/6 with OUT 6 doubling as watchdog — this chapter keeps that
arrangement. Earlier drafts of the assignment sketched read-shift "port
5?"; we resolved to the documented hardware mapping above.
