# provenance.md — ch10_lr35902_cpu goldens and fixtures

## Fixtures

`fixtures/*.bin` are course-original programs hand-assembled at `ORG $0100`
with the course mini-assembler (two-pass, label support; SM83 subset). The
exact source is the sibling `*.asm.txt`; the `.bin` files were generated from
those listings and never edited by hand. No commercial ROM content.

| File | Coverage |
|---|---|
| sm01_alu_ops.bin | ADD/ADC/SUB/SBC/AND/XOR/OR/CP incl. immediate forms, LDI stores |
| sm02_loads_loop.bin | pair loads, indirect A loads, LDI/LDD, JR loop, ADD HL,rr |
| sm03_cond_loops.bin | nested JR cc / JP cc loops |

## Golden traces

`traces/*.trace.log` produced by the reference solution runner:

```
python3 tools/labs/generate.py --mode solution --force --targets ch10_lr35902_cpu
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j
./build-solutions/solutions/ch10_lr35902_cpu/03_ld_alu/ch10_03_ld_alu_runner \
    --rom tests/public/ch10_lr35902_cpu/fixtures/sm01_alu_ops.bin \
    --headless --cycles 100000 \
    --trace tests/public/ch10_lr35902_cpu/traces/sm01_alu_ops.trace.log \
    --dump /dev/null
```

Each trace was generated twice and byte-compared (identical) before commit.
Trace line format: `pc= op= af= bc= de= hl= sp= cyc=` emitted after each
executed instruction.
