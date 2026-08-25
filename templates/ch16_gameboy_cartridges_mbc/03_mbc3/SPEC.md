# 03 — MBC3 with a deterministic injected-tick RTC

| seq | function       | contract |
|-----|----------------|----------|
| 1   | `Rtc::tick`    | 4194304 T-cycles = 1 s; carry chain secs->mins->hours->days->day-bit-8 (9-bit day counter); sub-second remainder drops per call; HALT (daysHi bit 6) freezes everything |
| 2   | `updateLatch`  | $00 arms, then $01 copies live RTC into shadows and freezes reads. Repeated $00s keep it armed; any other order resets the wait and must NOT latch |
| 3   | `writeReg`     | RAM enable window; ROM bank = low 7 bits of $2000-$3FFF value, 0 -> 1; $4000-$5FFF accepts only $00-$03 (RAM bank) or $08-$0C (RTC select), others ignored; $6000-$7FFF feeds the latch machine |
| 4   | `readRam`      | RTC selected -> register byte (shadow while frozen); else gated cart RAM |
| 5   | `writeRam`     | RTC writes hit LIVE registers always (even while frozen); SRAM writes mirror the read-side gating |

The clock never reads the host wall clock — time moves only when
`tick()` / the runner's `T` op injects cycles. That is what makes every
golden trace in this chapter reproducible.

## Acceptance

`ch16_03_mbc3_tests` passes: carry chain across seconds/minutes/hours/
days including the ninth day bit, HALT freeze + resume through the bus
writable daysHi register, latch handshake ordering (01-then-00 rejected,
00-then-00-then-01 accepted), frozen reads staying stale while the live
clock advances, 7-bit bank masking with modulo wrap.
