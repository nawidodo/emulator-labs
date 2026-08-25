# ch51 Coding Test — integrated capstone grading

Unlike every other chapter, there is nothing to implement *inside this
template*. The coding test grades an **externally built, student-owned
integrated emulator** through the hidden manifest:

```
tests/hidden/ch51_ps1_capstone/manifest.json
```

## Contract

* `binary` fields use the placeholder `{{env:LABS_CAPSTONE_BIN}}`.
  `tools/labs/grade.py` expands environment variables in `binary`,
  `args` and hash-file fields. With the variable **unset**, expansion
  resolves to the repo directory and the grader aborts with a
  permission error — always set `LABS_CAPSTONE_BIN` before grading.
* The referenced binary must speak the course-standard headless CLI
  (curriculum §52):

  ```
  --rom PATH --headless --cycles N --frames N
  --trace FILE --hash-frame FILE --input-file FILE
  ```

* Determinism is absolute: identical input produces byte-identical
  `--hash-frame` dumps and traces. No wall-clock, no RNG, no host paths.
* All fixtures are synthetic course material (`.bin` + `.asm.txt` +
  provenance). No commercial ROM ever enters the pipeline.

## Case catalogue

The example manifest (`manifest.example.json`, not executed by default)
defines the protocol/state checks run against `$LABS_CAPSTONE_BIN`:

| Case                        | Exercises                        | Evidence            |
|-----------------------------|----------------------------------|---------------------|
| capstone_cpu_trace          | ch38/39 CPU + COP0               | golden trace file   |
| capstone_dma_chain_state    | ch43 DMA                         | state dump hash     |
| capstone_gte_vector         | ch44 GTE                         | register dump hash  |
| capstone_timer_irq_order    | ch40 INTC + timers via scheduler | event-log hash      |
| capstone_cd_latency_read    | ch45 CD-ROM                      | sector dump hash    |
| capstone_mdec_block         | ch46 MDEC                        | decoded-block hash  |
| capstone_spu_stream         | ch47 SPU                         | PCM hash            |
| capstone_pad_transaction    | ch48 pad                         | response bytes hash |
| capstone_card_roundtrip     | ch48 memory card                 | .mcr image hash     |
| capstone_boot_milestones    | ch49 scheduler integration       | event-log hash      |

Each one is a direct reuse of the matching chapter's hidden-case shape —
if you passed chapters 38–50 with your own implementations, the capstone
cases are composition, not new research.

## Out-of-the-box case

`selfcheck.pipeline_ok` runs without any student binary. It exercises the
grading machinery itself: executable resolution from a committed script,
`{{tmp}}` scratch-dir expansion and FNV-1a-64 file hashing. It must be
GREEN on every machine at all times.

## Preparing

1. Finish the integration checklist in `../README.md`.
2. Rehearse each row against its source chapter's public goldens.
3. `LABS_CAPSTONE_BIN=/path/to/your/emulator python3 tools/labs/grade.py \
      --repo . ch51_ps1_capstone`
