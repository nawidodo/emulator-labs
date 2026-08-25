# Debugging exercise — ch43: linked-list chain bugs

A GPU linked-list DMA walker ships with **two seeded bugs**. The unit tests
in this directory run RED on the untouched skeleton. Your job:

1. Reproduce each failure and read the symptoms.
2. Locate the root cause (both live in `debug_ll.hpp`).
3. Produce `bug-report.md` containing, for EACH bug:
   - **bug** — one-sentence observable misbehavior
   - **root cause** — the exact line and why it is wrong
   - **first divergence** — the earliest test observation that contradicts
     hardware behavior (trace-first thinking: compare expected vs actual
     capture word-by-word)
   - **fix** — the corrected code
   - **regression test** — which of the provided tests now guards it (or
     add one)

## Symptom guide

- Test `terminates_on_exact_sentinel` fails: the walker either never
  reports termination or reports it at the wrong packet. Hardware
  terminates a GPU list ONLY when a header's low 24 bits are exactly
  `0FFFFFFh`. Any other value — including zero — is an address hop.

- Test `gpu_receives_payload_only_in_order` fails with a shifted capture:
  every packet delivers one extra leading word. Remember the division of
  labor: the DMA unit consumes the header; only payload words reach the
  device.

## Acceptance

- All tests in this directory GREEN after your fix.
- `bug-report.md` present with both entries.
