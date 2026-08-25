# Provenance — ch27_gba_memory_system public goldens

No ROM data is used anywhere in this chapter; all fixtures are synthetic
in-memory patterns built by the unit tests themselves.

## goldens/

- `calc_report.txt`: deterministic timing report from the reference
  solution (`ch27_calc_runner`), covering burst totals for WS0/WS1/WS2,
  EWRAM, IWRAM and SRAM plus the fastest-chip verdicts for bursts of 1
  and 16 halfwords.

The file was generated TWICE by the reference solution and byte-compared
before committing; both runs were identical. The hidden manifest hashes
(`tests/hidden/ch27_gba_memory_system/manifest.json`) were computed from
the same outputs (FNV-1a 64). The FLASHCART coding-test report follows the
same procedure via `ch27_flash_report`.
