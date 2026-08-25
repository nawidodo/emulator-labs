# provenance — ch11 public fixtures and goldens

Course-original programs assembled with the course mini-assembler
(`/tmp/labs-gb1/sm83asm.py`). No commercial ROM material. The `.asm.txt`
files are the authoritative listings; the committed `.bin` files are
generated verbatim from them — never hand-edit a `.bin`.

```bash
# smoke_cpu.bin (single section at ORG $0100; FlatBus.load places it at $0100)
python3 -c "import sys; sys.path.insert(0,'/tmp/labs-gb1'); from sm83asm import assemble; code,labels=assemble(open('smoke_cpu.asm.txt').read()); open('smoke_cpu.bin','wb').write(code)"

# smoke_irq.bin (single section at ORG $0100)
python3 -c "import sys; sys.path.insert(0,'/tmp/labs-gb1'); from sm83asm import assemble; code,labels=assemble(open('smoke_irq.asm.txt').read()); open('smoke_irq.bin','wb').write(code)"
```

Notes:

- The mini-assembler folds every `ld ($FFxx),a` into the E0 LDH form, which
  this chapter's core does not implement, so smoke_irq emits its IF/IE
  stores as raw `db $EA,...` opcodes.
- smoke_irq copies its 2-byte ISR template down to the timer vector `$0050`
  at runtime (the image itself stays above $0100).
- Absolute call/jp/rst targets are written as literal `$hex` addresses;
  only relative `jr` branches use labels.
- Traces under `../traces/` and every hash in `../goldens/goldens.md` were
  produced by running the reference implementation TWICE with byte-
  identical output before committing.
