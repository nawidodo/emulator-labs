# ch26 — Thumb, Pipeline and Exceptions

Decode Thumb formats 1–6, build an explicit fetch/decode/execute pipeline
with correct PC-reads semantics, and model exceptions with banked registers.

## Exercises

| Dir | Topic | Deliverable |
|-----|-------|-------------|
|01_thumb_decoder | format tables 1–6 + branch encodings | `decode()` |
|02_pipeline      | explicit fetch/decode/execute, PC = instr+4, literal pools | ThumbCpu + runner |
|03_mode_switch   | BX interworking, Thumb long BL, ARM BL (LR = pc+4) | dual-mode step |
|04_exceptions    | banked registers/modes table, SWI/IRQ/FIQ entry & return | exception unit |
|90_debug         | five seeded pipeline/PC bugs | DEBUGGING.md + bug-report.md |
|91_challenge     | PUSH/POP + ARM↔Thumb interleave fixture vs golden dump | CHALLENGE.md |
|99_coding_test   | pipeline from spec; hidden grader hashes exact traces | CODING_TEST.md |

## Runner

`02_pipeline`, `03_mode_switch`, `91_challenge` and `99_coding_test` build a
headless runner:

```bash
ch26_runner --rom prog.bin --headless --cycles N --trace t.log --dump d.txt
```

Trace lines: canonical `pc=<hex> op=<hex> cyc=<n> r0..r15 cpsr=<hex>` where
`op` is the 16-bit halfword in Thumb state. CPU-only chapter: `--frames`
behaves like `--cycles`.

## Verification

```
VERIFY_PREFIX=/tmp/labs-GBAF tools/labs/verify_chapter.sh \
    ch25_arm7tdmi_arm_isa ch26_gba_thumb_pipeline_exceptions
# -> skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch26_gba_thumb_pipeline_exceptions
# -> all non-optional hidden cases pass
```
