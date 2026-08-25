# Challenge — ch45: CD-ROM state transitions as transcripts

Real CD-ROM bugs are ordering bugs: an interrupt delivered one tick early,
a STAT bit cleared too soon, a second response racing the first. The
challenge harness turns controller behavior into pure text so every
ordering mistake is visible.

## Your task

`cd_session.hpp` implements a scripted session runner over the chapter's
controller. Extend/harden it so that:

1. Every command produces its documented first response (INT3) at the
   current guest tick, with the correct STAT byte.
2. Init/Pause/SeekL completions land on the EXACT documented ticks.
3. Streaming reads deliver INT1 lines carrying the LBA of the sector
   being delivered; Pause freezes the stream permanently.
4. Interrupts queue in issue order and surface in firing order.

## Log line format

```
t=<decimal> int=<0..5> resp=<HEX bytes '-'>-joined> lba=<decimal>
```

(`lba=` only on INT1 data-ready lines.)

## Acceptance

```bash
ch45_91_session_tests                       # determinism unit tests GREEN
ch45_cd_cli --cue tests/public/ch45_ps1_cdrom/disc/pub.cue \
    --bin  tests/public/ch45_ps1_cdrom/disc/pub.bin \
    --script tests/public/ch45_ps1_cdrom/scripts/pub.script
# output identical to scripts/pub_expected.log (see provenance.md)
```

The hidden grader replays an unseen script against a different disc and
hashes the transcript — any timing deviation breaks the hash.
