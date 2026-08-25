# ch49 fixture provenance

Synthetic fixtures only — no commercial ROMs. Programs are hand-assembled
from their `.asm.txt` listings against the mini-ISA encoding in
`templates/ch49_ps1_system_scheduling/02_mini_devices/core.hpp`; each
listing regenerates its `.bin` byte-for-byte.

Hashes below were produced by the reference solution runner, run TWICE per
scenario, byte-identical both times:

```bash
ch49_03_boot_runner_runner \
    --rom tests/hidden/ch49_ps1_system_scheduling/roms/unseen_boot_a.bin \
    --input-file tests/hidden/ch49_ps1_system_scheduling/roms/script_a.script \
    --cycles 60000 --hash-frame a.log --headless
# run 1 == run 2: fnv64=788C1334E66A2072

ch49_03_boot_runner_runner \
    --rom tests/hidden/ch49_ps1_system_scheduling/roms/unseen_boot_b.bin \
    --input-file tests/hidden/ch49_ps1_system_scheduling/roms/script_b.script \
    --cycles 60000 --hash-frame b.log --headless
# run 1 == run 2: fnv64=83342705CA5D58C9
```

Scenario intent:

- **unseen_boot_a** — GPU-before-CD handshake ordering with an SPU sample
  chain running throughout; `script_a.script` stalls the boot with a DMA
  kick so every subsequent deadline shifts by the drain length. Any FIFO
  tie-break defect reorders the latch lines inside contended dispatch
  batches and changes the hash.
- **unseen_boot_b** — delay-loop prologue, CD completion polled through
  I_STATUS while masked out of the INTC, GPU/GPUCMD and CD kicked by
  script at cycle 0. Exposes ordering defects between script-injected
  device events and the boot chain.
