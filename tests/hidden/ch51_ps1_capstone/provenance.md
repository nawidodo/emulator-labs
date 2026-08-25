# ch51 hidden manifest provenance

## selfcheck.pipeline_ok

The only executable case ships a committed, deterministic script
(`selfcheck/selfcheck.sh`) that writes a fixed payload to the `{{tmp}}`
scratch dir. Generated twice by running:

```bash
bash tests/hidden/ch51_ps1_capstone/selfcheck/selfcheck.sh /tmp/sc1.bin
bash tests/hidden/ch51_ps1_capstone/selfcheck/selfcheck.sh /tmp/sc2.bin
cmp /tmp/sc1.bin /tmp/sc2.bin   # byte-identical
```

FNV-1a 64 of `pipeline.bin`: `A97463F5F76BA55D`

Both runs byte-identical before pinning. The case additionally validates
that grade.py resolves a committed executable from a manifest `binary`
field, expands `{{tmp}}` args, and hashes the produced file — the exact
machinery the `{{env:LABS_CAPSTONE_BIN}}` grading flow depends on.

## env-gated capstone cases

Not executed here by design: they address the student's integrated binary
through `{{env:LABS_CAPSTONE_BIN}}`. The full case catalogue and example
manifest live in `templates/ch51_ps1_capstone/99_coding_test/`; fixture
ROMs/scripts are drawn from the synthetic course fixture set at final
grading time, with hashes pinned there (generated twice from reference
runs, per docs/AUTHORING.md golden policy).

## Manual env-gated flow probe

`{{env:LABS_CAPSTONE_BIN}}` expansion was exercised against grade.py's
own `run_case` with a stub executable at `/tmp/capstone_stub.sh`
(`args: ["--headless"]`, `expect_exit: 0`) — result `(True, 'ok')`.

With the variable UNSET, expansion resolves to the repo directory and
`subprocess.run` raises `PermissionError` — documented in README.md and
CODING_TEST.md as "always set the variable"; tools/ is shared
infrastructure and intentionally unmodified.
