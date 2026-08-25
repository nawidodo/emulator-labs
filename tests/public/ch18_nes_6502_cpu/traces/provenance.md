# provenance.md — ch18_nes_6502_cpu goldens

## challenge_golden.log

Canonical trace (`pc= op= a= x= y= p= sp= cyc=`) of the course-original
program `programs/challenge_prog.bin` (listing: `programs/challenge_prog.asm.txt`),
run by the ch18 reference solution CPU.

Generating command (run twice; outputs byte-identical):

```bash
build/skels/ch18_nes_6502_cpu/91_challenge/ch18_91_challenge_runner \
    --rom tests/public/ch18_nes_6502_cpu/programs/challenge_prog.bin \
    --data 2100=0f --cycles 100 \
    --trace tests/public/ch18_nes_6502_cpu/traces/challenge_golden.log
```

The fixture program is hand-assembled from the committed listing; no
commercial ROM content is involved. 30 instructions, ending on the
deliberate JAM marker at $0631.
