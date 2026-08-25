# Debugging exercise — ch45: CD-ROM seek bugs

Two seeded bugs live in `debug_cd.hpp`. Tests run RED until fixed.
Produce `bug-report.md` with bug / root cause / first divergence / fix /
regression test for each.

## Symptom guide

- `seek_lands_on_requested_sector` fails with a constant offset: the MSF
  → LBA conversion forgot that LBA numbering starts at the FIRST DATA
  SECTOR (MSF 00:02:00), i.e. the 150-frame lead-in must be excluded.

- `completion_respects_seek_latency` fails immediately: the drive reports
  seek completion at the very tick the command was issued. Head movement
  takes time — the documented lab model is `100 + |delta_lba|` ticks.

## Acceptance

All tests GREEN after fixes + complete `bug-report.md`.
