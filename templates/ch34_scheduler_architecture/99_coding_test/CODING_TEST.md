# Coding test — three-device synchronization (ch34)

Write a driver that synchronizes three fictional devices running at
different frequencies on one integer master clock and produces an exact
event-order log.

## Spec

Devices (all start at master cycle 0, first deadline = their period):

| device | period (guest cycles) |
|---|---|
| `a` | 3 |
| `b` | 5 |
| `c` | 7 |

Run the master clock to `--cycles N` (inclusive). Every dispatch appends
one line:

```text
cyc=<decimal> dev=<a|b|c>
```

Ties dispatch in scheduling order (device `a` is scheduled first, then
`b`, then `c`; each reschedule keeps its own relative order). The log is
written to `--out FILE`. On success print `events=<n>` to stdout and exit
0. With no events (or an unfinished scheduler), exit 1.

## Grading

The hidden manifest runs your binary against a fixed cycle budget and
hashes the produced log. Any deviation in ordering — including tie order
— fails the hash.

## Where to work

`sync3.cpp` in this directory contains annotated stubs (`@LABS` blocks 1–2
plus a final sanity block). Implement them, keep the CLI shape, and verify:

```bash
./ch34_99_sync3 --cycles 50 --out /tmp/ev.log && head /tmp/ev.log
```

Expected head: `cyc=3 dev=a`, `cyc=5 dev=b`, `cyc=6 dev=a`,
`cyc=7 dev=c`, ...
