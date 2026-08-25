# ch10_lr35902_cpu — SM83 CPU Core

Build the Game Boy CPU from the ground up: register model, instruction
metadata, decoder, and the base-page LD/ALU instruction set.

## Layout

| Dir | Content |
|---|---|
| `01_cpu_state/` | registers, flag packing, 16-bit pair views |
| `02_opcode_meta/` | `Instruction{name,bytes,cycles,cycles_alt}` tables (base + CB) |
| `03_ld_alu/` | decoder + executor, headless runner (`--rom --cycles --trace --dump`) |
| `90_debug/` | debugging exercise: 3 seeded bugs, see `DEBUGGING.md` |
| `91_challenge/` | CPU smoke-program suite vs golden dumps |
| `99_coding_test/` | unseen-spec coding test: LDH opcode family |

Exercises within a chapter may include earlier headers relatively
(`../02_opcode_meta/opcode_meta.hpp`); chapters never depend on other
chapters.

## Gate checklist

- [ ] exercises: skeletons RED -> your implementation GREEN
- [ ] starter: `make skels LABS=ch10_lr35902_cpu && make build && make test`
- [ ] debug: fix seeded bugs in `90_debug`, write `bug-report.md`
- [ ] challenge: `91_challenge` acceptance criteria met (golden match)
- [ ] coding test: hidden manifest `tests/hidden/ch10_lr35902_cpu/manifest.json` passing

## Verification

Recorded after authoring (see end-to-end log):

```
VERIFY_PREFIX=/tmp/labs-gb1 tools/labs/verify_chapter.sh ch10_lr35902_cpu
# verdict: skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch10_lr35902_cpu   # solution-built binaries
```

Golden traces/hashes under `tests/public/ch10_lr35902_cpu/` were produced by
the reference solution (run twice, byte-identical). See `provenance.md` there.
