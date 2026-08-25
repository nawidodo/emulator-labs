# Chapter 19 — NES CPU Accuracy

ch18 made a 6502 that runs programs. This chapter makes one that survives
comparison against real hardware: interrupt vector flows with the stacked-B
rule, dummy bus accesses (the RMW double-write, speculative indexed reads,
invisible internal ticks), a documented unofficial-opcode subset, and the
nestest-style trace logging that turns accuracy from an opinion into a diff.

## Exercises

| Dir | Task |
|---|---|
| `01_interrupts` | RESET/BRK/IRQ/NMI: padding-byte PC push, B bit set on BRK / clear on hardware flows, $FFFA vs $FFFE vectors, I-flag masking, edge-latched NMI polling, exact 7-cycle sequences |
| `02_dummy_unofficial` | dummy accesses as REAL bus transactions (RMW old-value write-back, speculative reads at un-fixed-up addresses, internal penalties) + unofficial NOP families, LAX/SAX, DCP/ISB/SLO/RLA |
| `03_trace_log` | nestest-style column formatter + instruction disassembler; runner gains `--trace-log` |

## Debug

`90_debug` — two seeded bugs: the RMW helper lost its old-value write-back
(cycles run short, snooping devices see one write instead of two) and NMI
polling became level-sensitive (holding the line storms the handler).
Students produce `bug-report.md`.

## Challenge

`91_challenge` — run the course-original `challenge_prog.bin` and match the
committed golden log line-by-line (51 nestest-style lines including cycle
counts): BRK/RTI round trip, unofficial RMW combos, speculative indexed
accesses, branches.

## Coding test

`99_coding_test` — implement `find_first_divergence()` and pinpoint the
single seeded difference in a committed 50,000-instruction trace pair at
exactly line 49867.

## Gate checklist

- [ ] exercises: skeleton RED -> student GREEN (all three dirs)
- [ ] starter: chapter generates and builds
- [ ] debug: `regression.*` green after fixing both bugs + bug-report.md
- [ ] challenge: trace-log matches golden line-by-line
- [ ] coding test: `unseen.*` green (incl. committed 50k pair)

## Verification

```
VERIFY_PREFIX=/tmp/labs-NESF tools/labs/verify_chapter.sh \
    ch19_nes_cpu_accuracy ch20_nes_bus_cartridges
[verify] SKEL: build OK; ctest: 29% tests passed, \
10 tests failed out of 14  (red failures expected here)
[verify] SOLUTIONS: GREEN — 100% tests passed out of 14
[verify] verdict: skel_build=ok solutions=GREEN

python3 tools/labs/grade.py --repo <scratch-repo> \
    ch19_nes_cpu_accuracy ch20_nes_bus_cartridges
== grade summary: 13/13 cases passed ==
```

Goldens in `tests/public/ch19_nes_cpu_accuracy/` were generated twice by
the reference solution (byte-identical); see `traces/provenance.md`.
nestest itself is referenced (not bundled) as the canonical validation ROM:
https://www.nesdev.org/wiki/Emulator_tests — the hidden manifest carries an
optional `requires_rom` case for it.
