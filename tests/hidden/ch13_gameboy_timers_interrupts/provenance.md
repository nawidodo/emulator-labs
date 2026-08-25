# ch13 hidden fixture provenance

`fixtures/h1_timer_variant.bin` is assembled from
`fixtures/h1_timer_variant.asm.txt` with the course mini-assembler:

    python3 -c "import sys; sys.path.insert(0,'/tmp/labs-gb1'); \
from sm83asm import assemble; \
code,labels=assemble(open('h1_timer_variant.asm.txt').read()); \
open('h1_timer_variant.bin','wb').write(code)"

Unseen variant of the public `timer_probe` used by the coding-test gate:
TMA=$F0, TAC=$05 (select 01 -> DIV bit 3, one tick per 16 T-cycles ->
one interrupt every 256 T-cycles), a DEC/JR poll loop before EI/HALT, and
an ISR that also writes a $5A marker byte to $FF81. Output: 285 bytes,
`isr`=0x050, `main`=0x100, loaded at base $0000.

Golden interrupt log produced by the reference solution runner (run twice,
byte-identical):

    ch13_91_timer_runner \
      --rom tests/hidden/ch13_gameboy_timers_interrupts/fixtures/h1_timer_variant.bin \
      --headless --cycles 30000 --trace irq.log

Summary line: `116 tima overflows, 116 interrupts serviced`;
FNV-1a-64 of the trace = `14EA01ACD8809EC2` (matches manifest case
`timer_log_golden`). The optional mooneye `timer.bin` case is an honest
skip gated on a student-supplied ROM; no such binary ships in this repo.
