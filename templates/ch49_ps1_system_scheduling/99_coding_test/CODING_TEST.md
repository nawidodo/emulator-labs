# ch49 coding test — hidden event-log contracts

Contract: given an unseen boot program (`.bin`, mini-ISA encoding per
`02_mini_devices/core.hpp`) and optionally a device script (`--input-file`,
grammar in `03_boot_runner/SPEC.md`), the repaired system must produce the
exact hashed EVENT LOG. The scenarios are built so that multiple devices
contend for the same dispatch batches; any ordering defect — a broken FIFO
tie-break, a deadline recomputed from "now" instead of the last anchor, or
a latch logged out of sequence — changes the hash and fails.

Entry point: `ch49_03_boot_runner_runner --rom <prog.bin> [--input-file
<script>] --cycles <n> --hash-frame <out> --headless`.

Scenarios live in `tests/hidden/ch49_ps1_system_scheduling/roms/` with
`.asm.txt` listings and provenance notes. The hidden manifest replays them
and pins `fnv64` of the event log; see CODING_TEST.md grading notes in
docs/AUTHORING.md ("hidden manifest schema").
