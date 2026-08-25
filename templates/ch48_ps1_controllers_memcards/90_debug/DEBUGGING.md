# ch48 Debugging — two seeded defects

Both bugs live in `90_debug/*.hpp` as `@LABS` stubs (namespace `siodbg`).
The tests run RED with the seeded code and GREEN once you fix it. For each
bug produce `bug-report.md` containing:

```text
bug:
root cause:
first observable divergence:
fix:
regression test:   (the TEST name that would have caught it)
```

## Bug 1 — checksums that almost work (`debug_memcard.hpp`)

Symptom: memory-card reads of most sectors return a checksum one byte off,
GETID answers `0x04` instead of the famous `0x84`, and writes whose payload
was corrupted in exactly the last byte are silently accepted. Reads of
sectors whose final data byte happens to be `0x00` look perfectly healthy —
which makes the bug maddeningly intermittent.

Hint: an XOR sum is only correct if it sees EVERY byte it claims to cover.
Count the iterations of the reduction loop against the range it advertises.

## Bug 2 — the pad never (or always) ACKs (`debug_pad.hpp`)

Symptom: the bus-level ACK handshake is inverted. STAT bit 7 reads high
while the pad is idle and drops precisely while data bytes are still owed.
A driver waiting for ACK after the ID low byte hangs forever; a driver that
polls "ACK released = done" finishes three bytes early.

Hint: check both endpoints of the condition. Which values of the sequence
counter mean "a response byte was just emitted AND another one is owed"?
