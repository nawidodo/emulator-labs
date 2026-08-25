# Debugging — ch26 Thumb pipeline

The `90_debug` core builds and runs, but produces wrong traces on real
Thumb programs. Five defects are seeded. Symptoms observed while running
the chapter's test programs (`tests/public/ch26_gba_thumb_pipeline_exceptions/`):

1. **Every other Thumb halfword is skipped on linear runs.** The fetch
   stage advances PC by the wrong stride, so instructions after the first
   come from odd addresses and the stream desyncs.
   (test: `bug1_thumb_fetch_stride_is_two`)

2. **Literal pool values come back shifted by one word.** Format-6 loads
   use an ARM-style base instead of the Thumb `instr + 4`; any `LDR rX,
   [PC, #imm]` with imm > 0 reads the wrong slot.
   (test: `bug2_literal_pool_base_is_instr_plus_4`)

3. **Conditional branches land one halfword short.** Loops drift backwards
   each iteration until they re-run setup code; targets are computed from
   the wrong pipeline slot.
   (test: `bug3_cond_branch_target_from_plus_4`)

4. **Backward BL calls jump into unmapped memory.** Forward calls land at
   huge offsets — the 23-bit BL offset's sign bit is ignored when the
   second halfword executes.
   (test: `bug4_bl_backward_offset_is_sign_extended`)

5. **After any SWI the flags never recover.** Returning from the handler
   restores the PC but leaves the supervisor CPSR in place — the saved
   status register is written but never read back.
   (test: `bug5_swi_return_restores_cpsr`)

## Deliverable

Fix all five in `debug_cpu.hpp`, then write `bug-report.md` with, per bug:

```text
bug:            <one line>
root cause:     <what the code did wrong>
first divergence: <the first instruction whose observable state differs>
fix:            <what you changed>
regression test:<which TEST above covers it>
```
