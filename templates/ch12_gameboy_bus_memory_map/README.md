# ch12 — Game Boy Bus and Memory Map

Pure bus mechanics, no CPU anywhere: how `Bus::read`/`Bus::write`
route across the SM83 address space using an ordered range->device
table instead of a giant switch.

## Layout

| Directory | Target | Subject |
|---|---|---|
| `01_range_table/` | `ch12_01_range_tests` | range lookup, read/write routing, unmapped-gap policy |
| `02_device_attach/` | `ch12_02_attach_tests` | the full machine map: cart, VRAM, ext-RAM, WRAM, OAM, I/O, HRAM, IE |
| `03_echo_alias/` | `ch12_03_echo_tests` | E000-FDFF as a translating window onto WRAM (`-$2000`) |
| `04_boot_rom/` | `ch12_04_boot_tests` | boot overlay at 0000-00FF, unmap on any FF50 write |
| `90_debug/` | `ch12_90_debug_tests` | three seeded bus defects + `DEBUGGING.md` drill |
| `91_challenge/` | `ch12_91_conformance_tests` | unseen "CourseBoy-II" map spec + conformance suite |
| `99_coding_test/` | `ch12_99_coding_test_tests` | second unseen variant "Tetra-8" (banked window, NUL shadow) |

Exercises are self-contained within the chapter and include each other
by relative path (`../01_range_table/bus.hpp`), exactly like the
chapter's own verify tree requires. No cross-chapter includes.

## Gate checklist

- [ ] exercises 01-04: skeleton builds with RED tests, then GREEN
- [ ] starter: chapter generates, builds, tests via the labs workflow
- [ ] debug: all three seeded defects fixed + `bug-report.md`
- [ ] challenge: CourseBoy-II conformance suite green (incl. hidden corners)
- [ ] coding test: Tetra-8 suite green (incl. hidden corners)

## Policies this chapter commits to

* Unmapped/open slots: reads `$00`, writes dropped (documented; other
  consoles in 91/99 deliberately differ).
* Echo: same silicon as WRAM C000-DDFF via exact `- $2000`; no buffer
  copies.
* Boot overlay: table-order priority; ANY FF50 write unmaps; the real
  DMG `$0050` execution trigger is documented but not modeled (no CPU).

## Verification

Authoring-time verification (2026-08-25), isolated per the chapter
workflow so sibling chapters are untouched:

```bash
VERIFY_PREFIX=/tmp/labs-gbfin12 tools/labs/verify_chapter.sh ch12_gameboy_bus_memory_map
# [verify] SKEL: build OK; ctest: 0% tests passed, 7 tests failed out of 7  (red expected)
# [verify] SOLUTIONS: GREEN — 100% tests passed out of 7
# [verify] verdict: skel_build=ok solutions=GREEN
```

Every non-optional hidden case was then executed manually against the
solution-tree binaries (`/tmp/labs-gbfin12/solution/build/tree/...`),
all exit 0:

| Hidden case | Binary | Filter | Result |
|---|---|---|---|
| range_routing | `ch12_01_range_tests` | `routing.` | PASS (8 tests) |
| attach_isolation | `ch12_02_attach_tests` | `attach.` | PASS (7 tests) |
| echo_alias_roundtrip | `ch12_03_echo_tests` | `echo.` | PASS (6 tests) |
| echo_boot_unmap | `ch12_04_boot_tests` | `boot.` | PASS (6 tests) |
| debug_regression_all_three_fixed | `ch12_90_debug_tests` | `debug_` | PASS (3 tests) |
| conformance_hidden | `ch12_91_conformance_tests` | `hidden.` | PASS |
| coding_variant_hidden | `ch12_99_coding_test_tests` | `hidden.` | PASS |

The optional Mooneye case (`roms/gb/mooneye/mem_timing.bin`,
`optional: true`) is skipped honestly when the student-supplied ROM is
absent. No binary goldens exist for this chapter; fixtures are
deterministic and compiled in (see `tests/public/ch12_gameboy_bus_memory_map/provenance.md`).
