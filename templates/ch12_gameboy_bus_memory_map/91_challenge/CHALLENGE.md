# Challenge — unseen memory map: "CourseBoy-II"

No walkthroughs exist for this machine; the spec below is complete and
authoritative. Implement `cb2::Cb2Map` in `challenge_map.hpp` (the six
`TODO` blocks) against the `cb2::Map` interface — identical in shape to
the exercise-01 `gbmap::Bus`. The public suites in `main.cpp`
(`TEST(cb2, ...)`) cover every rule; hidden grading replays extra
corner cases against the same spec, so match this document exactly.

## CourseBoy-II system map (authoritative)

| Range         | Region        | Rule |
|---------------|---------------|------|
| 0000-3FFF     | CART bank 0   | reads `rom[addr]`; writes ignored |
| 4000-7FFF     | mirror bank   | reads `rom[addr - $4000]` — an exact mirror of bank 0; writes ignored |
| 8000-87FF     | VRAM window   | 2 KiB window onto 4 KiB dual-port VRAM; the page selected by SYS bit 0 is visible; writes persist per page |
| 8800-BFFF     | closed        | open bus |
| C000-CFFF     | work RAM      | 4 KiB, power-on `$00` |
| D000-DFFF     | closed        | open bus |
| E000-EFFF     | RAM alias     | same cells as C000-CFFF, translation `addr - $2000` (E000->C000 ... EFFF->CFFF) |
| F000-FEFF     | closed        | open bus |
| FF00-FF0F     | SYS registers | only FF00 is implemented (below); other SYS addresses read `$00` and drop writes |
| FF10-FFFF     | closed        | open bus |

**SYS register FF00** — page select:
* write: bit 0 := `val & 1`; bits 1-7 are reserved and ignored
* read: bit 0 = current page, bits 1-7 read as 0

**Open bus**: reads return `$FF`, writes are dropped. (CourseBoy-II
ties its data strobes high on empty decode slots — deliberately the
opposite of this chapter's Game Boy `$00` policy.) ROM reads past the
end of a short image also return `$FF`.

## Worked examples

```text
W 4001 55       ; ignored: ROM half takes no writes
R 4001  -> rom[$0001]
R 7FFF  -> rom[$3FFF]     ; mirror edge
R 3FFF  -> rom[$3FFF]     ; fixed-side edge reads the SAME cell
W FF00 FE ; R FF00 -> 00   ; only bit 0 decodes
W FF00 01 ; R FF00 -> 01
R E123  -> cell at C123
R F000  -> FF              ; closed band, not alias
```

## Acceptance

1. All visible `TEST(cb2, ...)` suites pass.
2. Hidden corner suites (`TEST(hidden, ...)`) pass: mirror edges,
   page-select masking, per-page persistence, alias boundary exactness,
   a full sweep of all four closed bands, dead SYS registers, and
   short-image padding.

## Hints

* Route first, translate second: decide WHICH region an address hits
  before doing any offset math.
* The alias covers exactly 4 KiB (E000-EFFF). FDFF-style GB intuition
  does not carry over — recompute the edges from THIS table.
