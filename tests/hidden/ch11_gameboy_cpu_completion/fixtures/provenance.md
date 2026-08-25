# provenance — ch11 hidden fixtures

Course-original programs assembled with the course mini-assembler
(`/tmp/labs-gb1/sm83asm.py`). No commercial ROM material.

```bash
# smoke_hidden.bin (single section at ORG $0100)
python3 -c "import sys; sys.path.insert(0,'/tmp/labs-gb1'); from sm83asm import assemble; code,labels=assemble(open('smoke_hidden.asm.txt').read()); open('smoke_hidden.bin','wb').write(code)"

# ldh_probe.bin (single section at ORG $0100)
python3 -c "import sys; sys.path.insert(0,'/tmp/labs-gb1'); from sm83asm import assemble; code,labels=assemble(open('ldh_probe.asm.txt').read()); open('ldh_probe.bin','wb').write(code)"
```

ldh_probe exercises all five unseen $FF00-page opcodes (E0/F0/E2/F2/08)
with observable HRAM read-backs (F0/F2 into A/B/C) and a WRAM round-trip of
SP through `ld ($D000),SP` + two FA reads, ending in HALT. Golden hash of
its final-state dump lives in
`tests/public/ch11_gameboy_cpu_completion/goldens/goldens.md`.
