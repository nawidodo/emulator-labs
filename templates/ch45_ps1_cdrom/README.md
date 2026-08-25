# ch45_ps1_cdrom — PS1 CD-ROM controller

Exercises:

| Dir               | Topic                                                  |
|-------------------|--------------------------------------------------------|
| `01_disc_image`   | BIN/CUE parsing, MSF<->LBA, sector validation           |
| `02_controller`   | command/response FIFOs, INT1..INT5, Init/Pause phases   |
| `03_read_engine`  | SeekL latency + ReadN/double-speed streaming            |
| `90_debug`        | seeded bugs: missing lead-in bias, zero-latency seeks   |
| `91_challenge`    | scripted session runner with transcript equality        |
| `99_coding_test`  | unseen command sequences -> response/interrupt logs     |

Run:

```bash
LABS=ch45_ps1_cdrom make skels && make build && ctest --test-dir build -R ch45
python3 tools/labs/generate.py --mode solution --force --targets ch45_ps1_cdrom
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && \
  cmake --build build-solutions -j && ctest --test-dir build-solutions -R ch45
```

Gate checklist:

- [x] exercises: skel RED -> student GREEN
- [x] starter: skeleton tree configures and builds
- [x] debug: `90_debug` + `bug-report.md`
- [x] challenge: `91_challenge` transcript equality vs golden
- [x] coding test: hidden manifest passes (unseen sequences)

## Verification

```
VERIFY_PREFIX=/tmp/labs-PS1B tools/labs/verify_chapter.sh ch45_ps1_cdrom ...
[verify] verdict: skel_build=ok solutions=GREEN
```

Synthetic disc fixtures and their provenance live under
`tests/public/ch45_ps1_cdrom/`; hidden scripts and goldens under
`tests/hidden/ch45_ps1_cdrom/`.
