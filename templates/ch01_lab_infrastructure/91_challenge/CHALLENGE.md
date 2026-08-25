# CHALLENGE — ch01: multi-target generation

The real generator accepts several targets in one invocation:

```bash
LABS="ch01/a ch01/b ch01/c" make skels
```

This chapter ships three mini templates that make that flow concrete:

| target    | directory        | concept                          |
|-----------|------------------|----------------------------------|
| `ch01/a`  | `a_mini_endian/` | one little-endian load           |
| `ch01/b`  | `b_mini_dumper/` | hex-dump offset formatting       |
| `ch01/c`  | `c_mini_bits/`   | bit-field extraction             |

## How multi-target resolution works

`tools/labs/generate.py` splits each target on `/`. The first component
may be a chapter prefix (`ch01` matches `ch01_lab_infrastructure`); the
second may be an exercise prefix (`a` matches `a_mini_endian` because a
directory's name up to its first `_` is a valid alias). The Makefile
splits the `LABS` variable on whitespace/commas and loops.

## Tasks

1. Generate all three targets at once:
   ```bash
   LABS="ch01/a ch01/b ch01/c" make skels
   ```
   Inspect `skels/ch01_lab_infrastructure/a_mini_endian/` etc.
2. Build and run their tests (`make build && ctest -R mini`). RED until
   you fill each single `TODO(1)`.
3. **Hash-check your own generator** against the same fixtures. Sentinel
   copies of the three minis live in
   `99_coding_test/data/challenge_{a,b,c}/mini.hpp`; drive YOUR
   `03_generator_starter/generate_skel.py` over them in solution mode:
   ```bash
   python3 skels/ch01_lab_infrastructure/03_generator_starter/generate_skel.py \
       --template-dir <repo>/templates/ch01_lab_infrastructure/99_coding_test/data/challenge_a \
       --mode solution --out /tmp/challenge_a
   ```
   Run it twice into different output directories and diff — byte-identical,
   manifest included. Then compare each emitted file's sha256 against the
   table below.
4. Record what you did in `challenge-notes.md` (commands + observations).
5. The machine-checked form of step 3 runs automatically once your
   generator is complete:
   ```bash
   ./build/skels/ch01_lab_infrastructure/99_coding_test/ch01_99_coding_test challenge
   ```

## Expected hashes (solution mode)

| file                        | sha256                                                            |
|-----------------------------|-------------------------------------------------------------------|
| challenge_a/mini.hpp        | `ce43b0c0d0c3f48eb685bc7c1d68b74bb5cae7958eaf3b1d44ea295d8bd08b55` |
| challenge_a/manifest.json   | `3060dbcd056a5400a6fbd5bb4717193a287da93d76904be5ec1b254fd30e6fc5` |
| challenge_b/mini.hpp        | `1ae027033db4d7f5b276714cf19e1d84c74b4797494349366af71f23d2c93304` |
| challenge_b/manifest.json   | `490806d50298d74c6c29f6cc0bf60587bfd82ca257322b501cde6cd5b3cbc005` |
| challenge_c/mini.hpp        | `c324a54a91a72ab108cda7e16314f98ff4e015e9eba0bdb27c71eb7344036c48` |
| challenge_c/manifest.json   | `711a7477efdc13dd03803cdd769e79da3cfb85b47a2f8387d6e1388e2ec0ee0b` |

## Acceptance criteria

- [ ] `LABS="ch01/a ch01/b ch01/c" make skels` produces three skeleton
      trees; each builds and its test goes GREEN after you solve the one
      TODO per mini.
- [ ] Your student generator emits byte-identical trees across repeated
      runs for all three sentinel fixtures (challenge mode of the checker
      passes).
- [ ] All six sha256 values match the table above.
- [ ] `challenge-notes.md` written.
