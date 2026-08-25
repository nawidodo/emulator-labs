# 99 — Coding Test: Locate the First Divergence

Trace-first debugging (curriculum §54) lives or dies on one tool: given a
known-good log and a suspect log, find the FIRST line where they disagree.
Everything after that point is consequence, not cause.

Your trace-comparison logs are plain text files of instruction lines
(canonical `pc=… op=… cyc=…` format or nestest-style columns — the tool
does not care).

## The contract

```cpp
bool find_first_divergence(const std::string& path_a,
                           const std::string& path_b,
                           uint64_t* line_no, std::string* line_a,
                           std::string* line_b);
```

- Compare line-by-line from line 1; on the FIRST difference store the
  1-based line number and both lines; return true.
- Identical files return false with `*line_no == 0`.
- A length mismatch is a divergence at the first missing line; the short
  side reports the synthetic line `"<eof>"`.
- Unreadable files count as divergence.

## The committed fixtures

`tests/public/ch19_nes_cpu_accuracy/traces/good50k.log` and
`bad50k.log` are a 50,000-instruction pair produced by the reference core;
the bad log differs from the good one at EXACTLY one late line (#49867 —
its cycle count). Your scanner must pinpoint that line, not just detect
"some difference".

## Acceptance

- All `unseen.*` tests pass, including the committed-pair case with the
  exact line number 49867.
