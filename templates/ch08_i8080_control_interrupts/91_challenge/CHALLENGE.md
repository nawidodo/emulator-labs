# Challenge — Pass the Course-Original Diagnostic

`ch08_diag.bin` (103-byte padded image, listing and provenance in
`tests/public/ch08_i8080_control_interrupts/roms/`) is a hand-assembled
diagnostic that exercises the chapter's full control-flow subsystem:

- CALL/RET through a real stack (LXI SP, balanced frames)
- Conditional jumps both TAKEN and NOT taken, with correct timing
- The ALU matrix (ADI carry path, CPI comparisons)
- A combined checksum stored to memory before HLT

## Acceptance criteria

```bash
ctest --test-dir build -R ch08_91_challenge        # all green
```

and via the headless runner against the committed fixture:

```bash
./build/skels/ch08_i8080_control_interrupts/91_challenge/i8080_diag_runner \
    --rom tests/public/ch08_i8080_control_interrupts/roms/ch08_diag.bin \
    --cycles 10000 --trace /tmp/diag.log
# AF=1C02 BC=1B00 ... cyc=162
```

The trace must be byte-identical to the committed golden
(`tests/public/ch08_i8080_control_interrupts/traces/ch08_diag.log`) — use
`tools/labs/compare_trace.py` to localize any divergence.

## Going further

External diagnostics (TST8080.COM, CPUTEST.COM from the 8080 exorcizer
suite) are excellent follow-ups; they are student-supplied ROMs, gated as
optional in the hidden manifest, never committed.
