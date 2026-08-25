# Coding test — ch45: unseen command sequence → response/interrupt log

Implement a minimal event-driven CD controller **from this spec alone**.
Hidden grading replays command sequences you have never seen and compares
your transcript byte-for-byte.

## Spec — mini CD sequencer

Guest time starts at 0. Each issued command produces a first response at
the CURRENT tick; scheduled completions fire when `tick n` advances guest
time past their deadline.

### Commands

| Command | First response | Completion |
|---|---|---|
| GetStat | `int=3 resp=<STAT>` at t | none |
| Setloc mm:ss:ff (BCD params) | `int=3` at t | stores target LBA |
| Init | `int=3` at t | `int=2` at t+1200 |
| Pause | `int=3` at t | `int=2` at t+250 |
| ReadN | `int=3` at t | first sector `int=1` at t + seek + S, then every S |
| SeekL | `int=3` at t | `int=2` at t + seek |

with `seek = 100 + |target − current|` and `S = 50`.

### STAT byte

`bit1 motor-on` after Init's first response; `bit5 read` while reading;
cleared by Pause. Error paths are out of scope for this test.

### Log line format

```
t=<decimal> int=<level> resp=<hex bytes '-'>-joined> lba=<current LBA>
```

`lba=` is appended only on `int=1` lines. Multiple commands may queue
interrupts; they surface in issue order (one visible at a time is out of
scope here — print immediately in firing order).

## Deliverable

```bash
./ch45_ct_sequencer_tests                     # public unit tests GREEN
./ch45_ct_sequencer_tests SCRIPT LOG          # fixture mode:
#   runs the script against YOUR implementation and writes the
#   transcript to LOG; exits 0 iff it matches the EXPECTED file given
#   as argv[3].
```
