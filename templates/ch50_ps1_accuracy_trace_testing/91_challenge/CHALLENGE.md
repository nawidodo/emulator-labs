# ch50 91_challenge — the whole accuracy gate, in one process

`ch50_91_challenge_tests` is the in-process twin of the aggregate runner:

1. **every_builtin_check_passes** — the seven built-in psx-mini checks
   (CPU golden trace compare, VRAM hash, SPU sample hash, DMA/GTE/timer/
   CDROM state pins) all run green through the same suite engine the CLI
   uses.
2. **reruns_byte_identical** — two consecutive suite runs produce
   byte-identical report text. Determinism is the product; a flaky pass
   is a failure.
3. **report_hash_matches_golden** — the FNV-1a 64 of the exact report text
   (7 PASS lines + `== 7/7 checks passed`) matches the golden pinned in
   `shared/goldens.hpp`. Because the runner prints those exact bytes, a
   student binary that passes this test and a CLI run that prints the
   golden summary are the same claim.

Acceptance: all three tests green on the solution build.

## Golden provenance

The report hash was produced by the reference solution running twice;
both runs were byte-identical. See
`tests/public/ch50_ps1_accuracy_trace_testing/provenance.md` for the
generation commands and the full golden table.
