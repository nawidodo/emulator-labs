# ch50 90_debug — ten seeded accuracy regressions

Every block in `regressions.hpp` carries one historical bug of the kind that
ships when an emulator "boots a game" but is not *correct*. The STUB side is
the seeded defect; the SOLUTION side is the corrected behaviour. Each seed is
detected by exactly ONE suite test (`ch50_90_regress_tests seedNN`), which is
the whole point: a regression suite where every fix gets a pinned case.

Diagnose each defect, fix it, and record it in your `bug-report.md`
(bug / root cause / first divergence / fix / regression test).

## Seed 01 — ALU immediate not sign-extended (`exec_addi`)

* **Bug**: `addi` adds the raw 12-bit immediate instead of sign-extending it.
* **Symptom**: loop countdowns explode — `addi r2, r2, -1` turns a counter
  into `counter + 4095`, so loops run away or terminate immediately.
* **Detect**: `seed01_alu.addi_sign_extends_imm12`.

## Seed 02 — trace reports post-increment pc (`step_trace`)

* **Bug**: the golden-trace formatter prints the pc AFTER advancing past the
  instruction instead of the fetch address.
* **Symptom**: every line of a trace comparison against committed goldens
  diverges while the machine's visible behaviour is perfect — the classic
  "boots fine, suite red" trap.
* **Detect**: `seed02_trace.line_reports_entry_pc`.

## Seed 03 — rectangle fill loses its last column (`gpu_fill`)

* **Bug**: column loop condition excludes the final requested column.
* **Symptom**: sprites gain a one-pixel transparent seam on their right
  edge; VRAM hashes never match.
* **Detect**: `seed03_gpu_fill.covers_requested_width`.

## Seed 04 — blit ignores source stride (`blit_rows`)

* **Bug**: rows advance by the destination width instead of the source
  buffer stride, pulling padding bytes into VRAM.
* **Symptom**: textures narrower than their upload buffer arrive scrambled
  or diagonally sheared.
* **Detect**: `seed04_vram_blit.honours_source_stride`.

## Seed 05 — release quantum applied twice (`env_exp_release`)

* **Bug**: the exponential release subtracts the delta two times per step
  and can push levels negative instead of clamping at silence.
* **Symptom**: every sound's tail is half as long as the reference render;
  sample-hash corpora mismatch.
* **Detect**: `seed05_env.exponential_release_quantum_once`.

## Seed 06 — DMA block transfers one extra word (`dma_run_block`)

* **Bug**: after the counted words, one additional word is copied behind the
  device buffer.
* **Symptom**: memory corruption adjacent to every transfer buffer; device
  state pins fail with one word of garbage at the tail.
* **Detect**: `seed06_dma.transfers_exactly_wc_words`.

## Seed 07 — descriptor chain leaves channel enabled (`dma_run_chain`)

* **Bug**: enable is left set when the chain retires (should clear like the
  hardware), so polling drivers restart the chain from descriptor zero.
* **Symptom**: linked-list transfers loop forever or double-deliver data.
* **Detect**: `seed07_dma_chain.clears_enable_when_chain_retires`.

## Seed 08 — GTE shift applied per-term (`gte_mac_y`)

* **Bug**: each product is shifted >>12 BEFORE summing instead of shifting
  the dot product once.
* **Symptom**: projected vertices drift by whole pixels wherever fractional
  terms would have carried into each other.
* **Detect**: `seed08_gte.shifts_once_after_the_dot_product`.

## Seed 09 — timer target compare overshoots (`timer_tick`)

* **Bug**: reload fires on `cnt > target` instead of exact equality, so the
  hit lands one tick late and the period drifts by one count forever.
* **Symptom**: periodic interrupts are subtly slow; timer state pins miss
  both the counter value and the target-hit count.
* **Detect**: `seed09_timer.reloads_on_exact_target_match`.

## Seed 10 — CDROM BCD decode swaps nibbles (`cdrom_bcd_to_dec`)

* **Bug**: MSF fields are decoded low-nibble-first as the tens digit.
* **Symptom**: seeks land on the wrong minute whenever both digits differ
  (`0x59` decodes to 95 instead of 59).
* **Detect**: `seed10_cdrom.decodes_bcd_msf_fields`.
