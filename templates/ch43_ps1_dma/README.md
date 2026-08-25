# ch43_ps1_dma — PS1 DMA controller

Exercises:

| Dir                | Topic                                                    |
|--------------------|----------------------------------------------------------|
| `01_channels`      | channel regs, DPCR/DICR, register decode                 |
| `02_block_transfer`| burst vs slice modes, chopping windows, cycle model       |
| `03_linked_list`   | OTC builder + GPU list walker (exact-sentinel semantics) |
| `90_debug`         | seeded chain bugs: wrong sentinel + header-as-payload     |
| `91_challenge`     | push GPU command lists through DMA into hashed VRAM      |
| `99_coding_test`   | unseen-spec chain inspector with boundary edge cases     |

Run:

```bash
LABS=ch43_ps1_dma make skels && make build && ctest --test-dir build -R ch43
# reference solution:
python3 tools/labs/generate.py --mode solution --force --targets ch43_ps1_dma
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && \
  cmake --build build-solutions -j && ctest --test-dir build-solutions -R ch43
```

Gate checklist:

- [x] exercises: skel RED -> student GREEN
- [x] starter: skeleton tree configures and builds
- [x] debug: `90_debug` fixes seeded bugs + `bug-report.md`
- [x] challenge: `91_challenge` acceptance criteria met
- [x] coding test: hidden manifest passes

## Verification

Recorded after authoring (see chapter README bottom for commands):

```
VERIFY_PREFIX=/tmp/labs-PS1B tools/labs/verify_chapter.sh ch43_ps1_dma ...
[verify] verdict: skel_build=ok solutions=GREEN
```

Golden fixtures live under `tests/public/ch43_ps1_dma/` and
`tests/hidden/ch43_ps1_dma/`; provenance next to the goldens.
