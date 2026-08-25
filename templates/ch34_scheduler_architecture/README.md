# ch34_scheduler_architecture

Event-queue scheduling for emulator devices: a deterministic priority-queue
scheduler, integer guest clocks only (never host time), master vs
device-local clocks, and the catch-up model.

## Layout

| Dir | What |
|---|---|
| `01_event_queue/` | Build `sched::Scheduler`: `schedule(ts, fn)`, `step()`, `run_until(limit)` with FIFO tie-breaks. |
| `02_toy_soc_events/` | Convert the fx8 toy CPU + timer + UART into event-driven devices under one master clock; headless runner with `--program/--cycles/--trace/--events`. |
| `90_debug/` | Seeded bug: same-timestamp dispatch order is wrong. Write `bug-report.md`. |
| `91_challenge/` | Convert the legacy ch13-style down-counter timer to event scheduling, bit-exact vs the tick-driven original. |
| `99_coding_test/` | Synchronize three fictional devices at frequencies /3, /5, /7 into an exact event-order log. |

## Gate checklist

- [ ] exercises: skel RED -> your GREEN (`01_event_queue`, `02_toy_soc_events`)
- [ ] starter: builds, tests run
- [ ] debug: `bug-report.md` (bug / root cause / first divergence / fix / regression test)
- [ ] challenge: legacy-vs-event flag timelines identical
- [ ] coding_test: hidden manifest passes (`python3 tools/labs/grade.py --repo . ch34_scheduler_architecture`)

## Fixture provenance

All programs under `tests/public/ch34_scheduler_architecture/programs/` are
hand-assembled synthetic fx8 programs; see `provenance.md` there. No
commercial ROMs. External hardware suites (e.g. Mooneye timer tests) are
referenced by URL and gated as `requires_rom` optional cases only.

## Verification

Recorded after authoring (see README end of each exercise for commands):

```
VERIFY_PREFIX=/tmp/labs-ch34 tools/labs/verify_chapter.sh ch34_scheduler_architecture
# verdict: skel_build=ok solutions=GREEN
python3 tools/labs/generate.py --mode solution --force --targets ch34_scheduler_architecture
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j && \
  ctest --test-dir build-solutions --output-on-failure   # ALL GREEN
python3 tools/labs/grade.py --repo . ch34_scheduler_architecture
```
