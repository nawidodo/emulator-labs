# Debugging — ch25 ARM ISA

The `90_debug` core builds and runs, but produces wrong results on real
programs. Five defects are seeded. Symptoms observed while running the
chapter's test program (`tests/public/ch25_arm7tdmi_arm_isa/roms/`):

1. **Flags drift during immediate-constant sequences.** A `MOVS r0,#0x42`
   following a subtract that borrowed changes C even though rotate-0
   immediates must leave the carry untouched. (test:
   `bug1_rotate_zero_immediate_must_not_touch_carry`)

2. **64-bit addition loses its high word.** `ADDS` sets C correctly but the
   follow-up `ADCS` for the high half never receives it — multi-word counter
   values come out short by exactly one. (test: `bug2_adc_carries_the_c_flag`)

3. **Multiword subtraction is off in the low word only when C was clear.**
   SBC subtracts one too much or too little depending on the incoming carry
   — borrow polarity inverted. (test: `bug3_sbc_borrow_polarity`)

4. **Compare corrupts registers.** After `CMP r2,#7`, register r9 contains 7.
   Compare-family opcodes must set flags only. (test: `bug4_cmp_does_not_writeback`)

5. **Subroutines return into the weeds.** After any BL, LR holds pc+8 instead
   of the instruction after the branch, so `BX lr` lands two instructions
   past the call site. (test: `bug5_bl_return_address_is_pc_plus_4`)

## Deliverable

Fix all five in `debug_cpu.hpp`, then write `bug-report.md` with, per bug:

```text
bug:            <one line>
root cause:     <what the code did wrong>
first divergence: <the first instruction whose observable state differs>
fix:            <what you changed>
regression test:<which TEST above covers it>
```
