# SPEC — 03_generator_starter

Build YOUR OWN simplified version of `tools/labs/generate.py`. The five
`TODO(n)` checkpoints in `generate_skel.py` map to the starter-project task
from the curriculum:

| checkpoint | TODO block          | curriculum task            |
|-----------|----------------------|----------------------------|
| 1         | `discover_template`  | TODO1 discover template    |
| 2         | `copy_verbatim`      | TODO2 copy files           |
| 3         | `parse_template`     | TODO3 remove completed blocks |
| 4         | `render`             | TODO4 preserve target TODO |
| 5         | `build_manifest`     | TODO5 generate manifest    |

## Marker grammar (subset)

Identical to `docs/AUTHORING.md`: `@LABS-BEGIN <seq>` / `@LABS-SOLUTION` /
`@LABS-STUB` / `@LABS-END`, any comment prefix of `// # ; ! * "`, `<seq>`
unique per file. Emission rules:

- `--todo T` (skel mode): block `seq <= T` emits its SOLUTION lines,
  others emit STUB lines;
- no `--todo`: every block emits its STUB;
- `--mode solution`: every block emits its SOLUTION;
- text outside blocks is copied verbatim **in position**;
- malformed sequences (nested BEGIN, misplaced SOLUTION/STUB, unterminated
  block) exit with code 2 and a diagnostic on stderr.

### The `%` sentinel convention

Fixture templates shipped under this exercise's `data/` spell their markers
`%LABS-BEGIN` etc. instead of `@LABS-BEGIN`. Reason: the repository-level
generator rewrites every marker-bearing file it finds under `templates/`
when producing reference solutions, which would destroy raw fixtures stored
there. `%LABS-` is invisible to it, so fixtures survive verbatim into both
the skeleton and solution trees. Your generator treats `@LABS-` and
`%LABS-` as synonyms (the pre-written `MARKER_RE` already does).

A file is transformed iff its text contains `@LABS-BEGIN` or `%LABS-BEGIN`;
all other files are copied byte-exact.

## CLI contract

```
generate_skel.py --list
generate_skel.py (--template NAME | --template-dir PATH)
                 [--todo N] [--mode {skel,solution}] --out DIR
```

- `--list` prints the names of the directories under `data/`, sorted,
  one per line, exit 0.
- `--template NAME` resolves to `data/NAME` (exit != 0 with an `error:`
  message when missing); `--template-dir PATH` accepts any directory and
  uses its basename as the template name.
- Outputs are written under `--out DIR` preserving relative paths.
- Exit 0 on success.

## manifest.json contract

Written into `--out DIR` itself (not listed inside it):

```json
{
  "generator": "student-gen 1.0",
  "mode": "skel",
  "todo": null,
  "template": "fixture_a",
  "files": [
    {"path": "app.py.tpl", "action": "copy", "sha256": "..."}
  ]
}
```

- `mode`: `"skel"` or `"solution"`; `todo`: the raw `--todo` value or null.
- One entry per output file (`manifest.json` excluded), `action` one of
  `"skeleton"` (transformed in skel mode), `"solution"` (transformed in
  solution mode) or `"copy"`; `sha256` is the hex digest of the written
  bytes.
- Entries sorted by `path`; serialization exactly `json.dumps(manifest,
  indent=2) + "\n"`.

**Determinism:** no timestamps, no absolute paths, deterministic walk
order. Running the tool twice over the same inputs must produce
byte-identical output trees, manifest included. This property is what the
hidden coding test checks.

## Fixture templates

- `fixture_a` — 3 sequential checkpoints, one Python file.
- `fixture_b` — 4 checkpoints across two C++ headers (per-file numbering).
- `fixture_c` — 2 checkpoints, mixed comment prefixes (`;` and none), a
  nested subdirectory, and marker-free files copied verbatim.

## Acceptance

`ctest -R ch01_03_generator` runs `test_generator.py`, which drives your
script through every `(fixture, level)` variant and compares each emitted
file's sha256 plus the manifest against recorded goldens. Skeleton RED,
solution GREEN — and the hidden chapter manifest re-checks your generator
against the unseen seven-level template.
