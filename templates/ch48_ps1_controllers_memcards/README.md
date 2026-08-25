# ch48 — PS1 Controllers, Memory Cards and Serial I/O

Build the SIO stack from the wire up: digital pad protocol, memory-card
command engine, `.mcr` card images with directory bad-block flags, and a
dual-slot bus scripted by a headless runner.

## Layout

| Dir              | Exercise                                                        |
|------------------|-----------------------------------------------------------------|
| `01_digital_pad` | pad device, 0x42 read transaction, button packing, ACK timing   |
| `02_card_protocol` | card state machine: 0x52/0x57/0x53/0x43, XOR checksums, flag sequence |
| `03_card_image`  | `.mcr` image + directory scan, dual-slot bus wiring + runner    |
| `90_debug`       | two seeded bugs: XOR over the wrong range, inverted pad ACK     |
| `91_challenge`   | virtual card round trip pinned to a golden FNV hash             |
| `99_coding_test` | hidden SIO transaction log → exact response bytes               |

## Gate checklist

- [ ] exercises: all `ch48_*_tests` RED on skeleton, GREEN when solved
- [ ] starter: `LABS=ch48_ps1_controllers_memcards make skels && make build && make test`
- [ ] debug: fix both seeded bugs, write `bug-report.md`
- [ ] challenge: `ch48_91_challenge_tests` GREEN
- [ ] coding_test: hidden manifest passes (`make grade GRADE_TARGETS=ch48_ps1_controllers_memcards`)

## Verification

Verified with the isolated chapter harness:

```bash
VERIFY_PREFIX=/tmp/labs-ps1fin-ch48 tools/labs/verify_chapter.sh ch48_ps1_controllers_memcards
# [verify] SKEL: build OK; red failures expected here
# [verify] SOLUTIONS: GREEN
# [verify] verdict: skel_build=ok solutions=GREEN
```

Hidden cases were additionally executed directly against scratch-built
solution binaries with the exact manifest arguments; see
`tests/hidden/ch48_ps1_controllers_memcards/provenance.md`.
