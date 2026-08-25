# ch27 — GBA Memory System

Route the GBA's 32-bit bus: nine regions, three bus widths, ARM rotation
semantics on unaligned reads, wait-state cycle accounting, and the
`BusResult{value, cycles}` interface.

## Exercises

| Dir | Topic | Deliverable |
|-----|-------|-------------|
|01_region_routing | address decode for all nine regions, VRAM mirror discontinuity | `Region` + `route()` |
|02_widths_alignment | 8/16/32 accesses, unaligned word rotation | width-aware read/write |
|03_waitstates | N/S cost table, sequential tracking, sequence totals | cycle accounting unit tests |
|04_bus_result | `BusResult{value,cycles}` integration across the bus | the reference Bus |
|90_debug | five seeded timing/routing bugs (post-branch S-cost, VRAM mirror misroute...) | DEBUGGING.md + bug-report.md |
|91_challenge | unseen access-sequence timing calculator matching the tables exactly | CHALLENGE.md |
|99_coding_test | unseen region timing spec: implement a new chip from its datasheet lines | CODING_TEST.md |

## Verification

```
VERIFY_PREFIX=/tmp/labs-GBAF2 tools/labs/verify_chapter.sh ch27_gba_memory_system
# -> skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch27_gba_memory_system
# -> all non-optional hidden cases pass
```
