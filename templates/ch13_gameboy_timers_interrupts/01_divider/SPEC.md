# 13.01 — The divider

Build the free-running 16-bit counter behind DIV ($FF04) and pin its
register semantics. The copied chapter CPU interface (bus.hpp, cpu.hpp,
int_ctl.hpp) lives in this directory; later exercises include it via
`../01_divider/*.hpp`.

## Contract

* One internal `uint16_t counter`; the visible DIV register is its HIGH
  byte only.
* Time advances in whole 4-T-cycle blocks (`step(cycles)` consumes
  `cycles/4` blocks; sub-block remainders never occur on SM83).
* Any write to DIV resets the ENTIRE internal counter (low bits included).
* DIV changes value once every 256 T-cycles (16384 Hz).

## Target

`ch13_01_divider_tests`
