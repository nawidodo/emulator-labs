# Debugging exercise — same-timestamp dispatch order (ch34)

## Symptom

`sched.equal_timestamps_fifo` and `sched.handler_may_schedule_reentrantly`
fail. On real machines the failure is nastier: an event-order golden log
mismatches even though every timestamp in it is *correct* — only the order
of events sharing a deadline is wrong. The UART byte enqueued first is
transmitted second; a timer IRQ delivered on the same tick as a DMA
completion arrives after it.

## Reproduce

```bash
LABS=ch34_scheduler_architecture/90_debug make skels && make build && \
  ctest --test-dir build -R ch34_90_debug --output-on-failure
```

## Hints (progressive)

1. The timestamps logged are all correct — this is NOT a time-arithmetic
   bug.
2. `std::priority_queue` ordering is fully determined by your comparator;
   heap internals give NO stability of their own.
3. Look at how ties are broken when `timestamp == rhs.timestamp`.

## Your task

Fix `scheduler.hpp`, then write `bug-report.md` containing:

```text
bug:        <one line>
root cause: <what the comparator does wrong and why the heap makes it
            nondeterministic-looking>
first divergence: <the earliest observable wrong dispatch>
fix:        <what you changed>
regression test: <which TEST() now guards it>
```
