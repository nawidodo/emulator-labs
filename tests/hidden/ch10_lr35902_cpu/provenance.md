# provenance.md — ch10_lr35902_cpu hidden fixtures

`roms/ldh_probe.bin` is a course-original program assembled at `ORG $0100`
from the sibling `ldh_probe.asm.txt` listing. It exercises the LDH opcode
family (E0/F0/E2/F2) plus SP-relative arithmetic (E8/F8) end-to-end.

Golden trace hash in `manifest.json` (`190366A9CCFB6BF9`, FNV-1a 64 of the
`--trace` output) was produced twice with the reference solution runner
(03 runner + LDH extension installed via `extra_exec`) and compared
byte-for-byte:

```
af=8810 bc=8840 de=00D8 hl=FFD0 sp=FFF0 pc=0119 cyc=132 halted=1 trap=0
```

No commercial ROM content.
